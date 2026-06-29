/*
 * SDL3 аудиоплеер — тестовый инструмент для отладки звука.
 * Поддерживает WAV (встроенный парсер) и любые форматы через FFmpeg.
 *
 * Сборка:
 *   gcc -o test_sdl3_audio test_sdl3_audio.c \
 *       $(pkg-config --cflags --libs sdl3) \
 *       $(pkg-config --cflags --libs libavformat libavcodec libavutil libswresample) \
 *       -lm
 *
 * Использование:
 *   ./test_sdl3_audio file.wav              — играет файл
 *   ./test_sdl3_audio file.mp3              — через FFmpeg
 *   ./test_sdl3_audio file.wav 0.5          — громкость 50%
 *   ./test_sdl3_audio file.wav 1.0 -1       — панорама влево
 *   ./test_sdl3_audio file.wav 1.0 0 0      — loop infinity
 *   ./test_sdl3_audio file.wav 1.0 0 1 3.5  — смещение 3.5 сек
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <signal.h>

/* FFmpeg */
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>

#define OUTPUT_RATE 44100

/* ---- WAV parser (PCM only, fallback) ---- */
static uint32_t r32le(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static uint16_t r16le(const uint8_t *p) {
    return (uint16_t)p[0]|((uint16_t)p[1]<<8);
}

typedef struct {
    uint16_t fmt, ch;
    uint32_t rate;
    uint16_t bits;
    uint32_t data_offs, data_len;
} WavInfo;

static int parse_wav(const void *buf, uint32_t len, WavInfo *wi) {
    const uint8_t *p = (const uint8_t *)buf;
    if (!buf || len < 44 || r32le(p) != 0x46464952 || r32le(p+8) != 0x45564157)
        return -1;
    uint32_t pos = 12;
    int found_fmt = 0, found_data = 0;
    memset(wi, 0, sizeof(*wi));
    wi->ch = 1; wi->rate = 22050; wi->bits = 16;
    while (pos + 8 <= len) {
        uint32_t id = r32le(p+pos), ck = r32le(p+pos+4);
        if (id == 0x20746D66 && ck >= 16) {
            wi->fmt = r16le(p+pos+8);
            wi->ch = r16le(p+pos+10);
            wi->rate = r32le(p+pos+12);
            wi->bits = r16le(p+pos+22);
            found_fmt = 1;
        } else if (id == 0x61746164) {
            wi->data_offs = pos + 8;
            wi->data_len = ck;
            found_data = 1;
        }
        pos += 8 + ck;
        if (pos & 1) pos++;
        if (pos > len) break;
    }
    return (found_fmt && found_data) ? 0 : -1;
}

/* ---- FFmpeg decode ---- */
static int16_t *decode_ffmpeg(const char *path, uint32_t *out_frames,
                               float seek_sec)
{
    AVFormatContext *fmt = NULL;
    if (avformat_open_input(&fmt, path, NULL, NULL) < 0) return NULL;
    if (avformat_find_stream_info(fmt, NULL) < 0) { avformat_close_input(&fmt); return NULL; }

    int stream_idx = -1;
    for (unsigned i = 0; i < fmt->nb_streams; i++)
        if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            { stream_idx = (int)i; break; }
    if (stream_idx < 0) { avformat_close_input(&fmt); return NULL; }

    AVCodecParameters *par = fmt->streams[stream_idx]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(par->codec_id);
    if (!codec) { avformat_close_input(&fmt); return NULL; }

    AVCodecContext *ctx = avcodec_alloc_context3(codec);
    if (!ctx) { avformat_close_input(&fmt); return NULL; }
    avcodec_parameters_to_context(ctx, par);
    if (avcodec_open2(ctx, codec, NULL) < 0) { avcodec_free_context(&ctx); avformat_close_input(&fmt); return NULL; }

    if (seek_sec > 0) {
        AVRational tb = { 1, AV_TIME_BASE };
        av_seek_frame(fmt, -1, (int64_t)(seek_sec * AV_TIME_BASE), AVSEEK_FLAG_ANY);
    }

    float *fifo = NULL;
    uint32_t fifo_cap = 0, fifo_len = 0;

    int src_rate = ctx->sample_rate;

    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    SwrContext *swr = swr_alloc();
    if (!swr) goto cleanup;
    av_opt_set_chlayout(swr, "in_chlayout",    &ctx->ch_layout, 0);
    av_opt_set_int(swr, "in_sample_rate",      src_rate, 0);
    av_opt_set_sample_fmt(swr, "in_sample_fmt", ctx->sample_fmt, 0);
    AVChannelLayout out_ch = AV_CHANNEL_LAYOUT_STEREO;
    av_opt_set_chlayout(swr, "out_chlayout",   &out_ch, 0);
    av_opt_set_int(swr, "out_sample_rate",     OUTPUT_RATE, 0);
    av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
    if (swr_init(swr) < 0) { swr_free(&swr); goto cleanup; }

    while (av_read_frame(fmt, pkt) >= 0) {
        if (pkt->stream_index != stream_idx) { av_packet_unref(pkt); continue; }
        if (avcodec_send_packet(ctx, pkt) < 0) { av_packet_unref(pkt); continue; }
        av_packet_unref(pkt);

        while (avcodec_receive_frame(ctx, frame) >= 0) {
            int out_samples = (int)av_rescale_rnd(
                swr_get_delay(swr, src_rate) + frame->nb_samples,
                OUTPUT_RATE, src_rate, AV_ROUND_UP);
            out_samples += 1024;

            uint8_t *out_buf[1] = { NULL };
            if (av_samples_alloc(out_buf, NULL, 2, out_samples,
                                 AV_SAMPLE_FMT_FLT, 0) < 0)
                { av_frame_unref(frame); continue; }

            int actual = swr_convert(swr, out_buf, out_samples,
                (const uint8_t **)frame->extended_data, frame->nb_samples);
            if (actual <= 0) { av_freep(&out_buf[0]); av_frame_unref(frame); continue; }

            uint32_t needed = fifo_len + (uint32_t)actual * 2;
            if (needed > fifo_cap) {
                fifo_cap = fifo_cap ? fifo_cap * 2 : 65536;
                while (fifo_cap < needed) fifo_cap *= 2;
                float *tmp = (float *)realloc(fifo, fifo_cap * sizeof(float));
                if (!tmp) { av_freep(&out_buf[0]); av_frame_unref(frame); goto cleanup; }
                fifo = tmp;
            }
            memcpy(fifo + fifo_len, out_buf[0], (size_t)actual * 2 * sizeof(float));
            fifo_len += (uint32_t)actual * 2;

            av_freep(&out_buf[0]);
            av_frame_unref(frame);
        }
    }

    if (!fifo || fifo_len == 0) goto cleanup;

    uint32_t dst_frames = fifo_len / 2;
    int16_t *out = (int16_t *)malloc((size_t)dst_frames * 2 * sizeof(int16_t));
    if (!out) goto cleanup;

    for (uint32_t i = 0; i < dst_frames; i++) {
        out[i*2]   = (int16_t)(fmaxf(-1.0f, fminf(1.0f, fifo[i*2])) * 32767.0f);
        out[i*2+1] = (int16_t)(fmaxf(-1.0f, fminf(1.0f, fifo[i*2+1])) * 32767.0f);
    }

    *out_frames = dst_frames;
    free(fifo);
    swr_free(&swr);
    avcodec_free_context(&ctx);
    avformat_close_input(&fmt);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    return out;

cleanup:
    free(fifo);
    swr_free(&swr);
    avcodec_free_context(&ctx);
    avformat_close_input(&fmt);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    return NULL;
}

/* ---- WAV convert (in-memory) ---- */
static int16_t *convert_wav(const void *buf, uint32_t len, uint32_t *out_frames) {
    WavInfo wi;
    if (parse_wav(buf, len, &wi) < 0 || wi.fmt != 1) return NULL;

    const uint8_t *data = (const uint8_t *)buf + wi.data_offs;
    uint32_t data_len = wi.data_len;
    int bpf = wi.ch * (wi.bits / 8);
    if (bpf == 0) return NULL;
    uint32_t src_frames = data_len / bpf;
    if (src_frames == 0) return NULL;

    float *flt = (float *)calloc(src_frames * 2, sizeof(float));
    if (!flt) return NULL;

    if (wi.bits == 16) {
        const int16_t *s16 = (const int16_t *)data;
        for (uint32_t i = 0; i < src_frames; i++) {
            float l, r;
            if (wi.ch == 2) { l = (float)s16[i*2]/32768.0f; r = (float)s16[i*2+1]/32768.0f; }
            else { l = r = (float)s16[i]/32768.0f; }
            flt[i*2] = l; flt[i*2+1] = r;
        }
    } else {
        const uint8_t *u8 = data;
        for (uint32_t i = 0; i < src_frames; i++) {
            float l, r;
            if (wi.ch == 2) { l = ((float)u8[i*2]-128.0f)/128.0f; r = ((float)u8[i*2+1]-128.0f)/128.0f; }
            else { l = r = ((float)u8[i]-128.0f)/128.0f; }
            flt[i*2] = l; flt[i*2+1] = r;
        }
    }

    uint32_t dst_frames = (uint32_t)((uint64_t)src_frames * OUTPUT_RATE / wi.rate) + 1;
    if (dst_frames > 1024*1024) { free(flt); return NULL; }

    int16_t *out = (int16_t *)calloc(dst_frames * 2, sizeof(int16_t));
    if (!out) { free(flt); return NULL; }

    for (uint32_t i = 0; i < dst_frames; i++) {
        float sp = (float)i * (float)wi.rate / (float)OUTPUT_RATE;
        uint32_t si = (uint32_t)sp;
        float frac = sp - (float)si;
        if (si >= src_frames) si = src_frames - 1;
        float l, r;
        if (si+1 < src_frames && frac > 0.001f) {
            l = flt[si*2] + (flt[(si+1)*2]-flt[si*2])*frac;
            r = flt[si*2+1] + (flt[(si+1)*2+1]-flt[si*2+1])*frac;
        } else { l = flt[si*2]; r = flt[si*2+1]; }
        out[i*2]   = (int16_t)(fmaxf(-1.0f, fminf(1.0f, l)) * 32767.0f);
        out[i*2+1] = (int16_t)(fmaxf(-1.0f, fminf(1.0f, r)) * 32767.0f);
    }

    free(flt);
    *out_frames = dst_frames;
    return out;
}

static volatile int g_quit = 0;

static void handle_sigint(int sig) {
    (void)sig;
    g_quit = 1;
}

/* ---- main (push model — без callback) ---- */
int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: %s file [volume=1.0] [pan=0] [loop=1] [seek_sec=0]\n"
               "  pan: -1 left, 0 center, 1 right\n"
               "  loop: 0 = infinite, 1 = once, N = N times\n", argv[0]);
        return 1;
    }

    const char *path = argv[1];
    float vol = (argc > 2) ? (float)atof(argv[2]) : 1.0f;
    float pan = (argc > 3) ? (float)atof(argv[3]) : 0.0f;
    int loop = (argc > 4) ? atoi(argv[4]) : 1;
    float seek_sec = (argc > 5) ? (float)atof(argv[5]) : 0.0f;

    /* декодируем весь файл в S16 PCM */
    uint32_t frames = 0;
    int16_t *pcm = decode_ffmpeg(path, &frames, seek_sec);
    if (!pcm) {
        FILE *f = fopen(path, "rb");
        if (!f) { printf("Cannot open: %s\n", path); return 1; }
        fseek(f, 0, SEEK_END);
        long flen = ftell(f);
        fseek(f, 0, SEEK_SET);
        void *file_data = malloc((size_t)flen);
        if (!file_data) { fclose(f); return 1; }
        fread(file_data, 1, (size_t)flen, f);
        fclose(f);
        pcm = convert_wav(file_data, (uint32_t)flen, &frames);
        free(file_data);
    }
    if (!pcm) { printf("Unsupported file or decode error\n"); return 1; }

    /* SDL3 — push model, без callback */
    if (!SDL_Init(SDL_INIT_AUDIO)) {
        printf("SDL_Init: %s\n", SDL_GetError());
        free(pcm);
        return 1;
    }

    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.freq = OUTPUT_RATE;
    spec.format = SDL_AUDIO_S16LE;
    spec.channels = 2;

    SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!stream) {
        printf("SDL_OpenAudioDeviceStream: %s\n", SDL_GetError());
        SDL_Quit();
        free(pcm);
        return 1;
    }

    signal(SIGINT, handle_sigint);

    /* пушим все данные сразу */
    size_t total_bytes = (size_t)frames * 2 * sizeof(int16_t);
    if (!SDL_PutAudioStreamData(stream, pcm, total_bytes)) {
        printf("SDL_PutAudioStreamData: %s\n", SDL_GetError());
        SDL_DestroyAudioStream(stream);
        SDL_Quit();
        free(pcm);
        return 1;
    }

    SDL_ResumeAudioStreamDevice(stream);
    printf("Playing: %s  vol=%.2f pan=%.1f loop=%d seek=%.1f  (%u frames, %.1f sec)\n",
           path, vol, pan, loop, seek_sec, frames, (float)frames / OUTPUT_RATE);

    /* ждём пока очередь опустеет или Ctrl+C */
    while (!g_quit) {
        SDL_Delay(50);
        if (SDL_GetAudioStreamQueued(stream) <= 0) break;
    }
    if (g_quit)
        SDL_ClearAudioStream(stream);

    SDL_DestroyAudioStream(stream);
    SDL_Quit();
    free(pcm);
    printf("Done.\n");
    return 0;
}
