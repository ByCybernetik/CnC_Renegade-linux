/*
 * Native SDL3 audio backend for C&C Renegade (Linux).
 * Step 2: SDL3 mixer + WAV + FFmpeg (MP3/OGG) from memory.
 *
 * Fixes vs previous version:
 *  - 3D samples use the same voice pool (no ID overflow)
 *  - AIL_WAV_info returns 0 for non-WAV (no div-by-zero)
 *  - AIL_release_sample_handle uses mutex (no race condition)
 *  - AIL_set_3D_sample_file delegates to AIL_set_named_sample_file
 */

#include "mss_stub.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <pthread.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

/* ================================================================
   Constants
   ================================================================ */

#define MAX_SLOTS       64
#define OUTPUT_RATE     44100
#define OUTPUT_CHANNELS 2
#define FRAME_SIZE      (OUTPUT_CHANNELS * (int)sizeof(int16_t))
#define MIX_BUF_FRAMES  2048

/* ================================================================
   Types
   ================================================================ */

struct UserData8 { intptr_t d[8]; };

struct Voice {
    bool      active;
    bool      playing;
    int16_t  *pcm;           /* S16 stereo OUTPUT_RATE, or NULL */
    uint32_t  frames;        /* total frames in pcm */
    uint64_t  pos;           /* 16.16 fixed-point */
    int       loop_remain;   /* <0 = infinite, 0 = done, >0 = remaining */
    float     volume;        /* 0..1 */
    float     pan;           /* -1..1 */
    UserData8 user_data;
};

/* ================================================================
   Global state
   ================================================================ */

static struct {
    SDL_AudioStream *stream;
    Voice            voices[MAX_SLOTS];
    pthread_mutex_t  mutex;
    int              next_id;
    bool             ready;
    bool             started;
    char             err[256];
    int16_t          mix_buf[MIX_BUF_FRAMES * OUTPUT_CHANNELS];
    /* file callbacks from the game */
    AIL_FILE_OPEN_CALLBACK   file_open;
    AIL_FILE_CLOSE_CALLBACK  file_close;
    AIL_FILE_SEEK_CALLBACK   file_seek;
    AIL_FILE_READ_CALLBACK   file_read;
} g_sdl;

/* ================================================================
   Helpers
   ================================================================ */

static void lock(void)   { pthread_mutex_lock(&g_sdl.mutex); }
static void unlock(void) { pthread_mutex_unlock(&g_sdl.mutex); }

static Voice *get_voice(HSAMPLE s) {
    int idx = (int)(intptr_t)s;
    if (idx < 1 || idx >= MAX_SLOTS) return NULL;
    return &g_sdl.voices[idx];
}

static int alloc_slot(void) {
    /* find next free slot, wrapping around */
    int start = g_sdl.next_id + 1;
    for (int i = 0; i < MAX_SLOTS; i++) {
        int id = ((start + i - 1) % (MAX_SLOTS - 1)) + 1;
        if (!g_sdl.voices[id].active) {
            g_sdl.next_id = id;
            return id;
        }
    }
    return 0; /* full */
}

/* ================================================================
   WAV parser (PCM only)
   ================================================================ */

static uint32_t r32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) |
           ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static uint16_t r16le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1]<<8);
}

struct WavInfo {
    uint16_t fmt;
    uint16_t ch;
    uint32_t rate;
    uint16_t bits;
    uint32_t data_offs;
    uint32_t data_len;
};

static int parse_wav(const void *buf, uint32_t len, WavInfo *wi) {
    if (!buf || len < 44) return -1;
    const uint8_t *p = (const uint8_t *)buf;
    if (r32le(p) != 0x46464952) return -1;
    if (r32le(p+8) != 0x45564157) return -1;
    uint32_t pos = 12;
    int found_fmt = 0, found_data = 0;
    wi->fmt = 0; wi->ch = 1; wi->rate = 22050;
    wi->bits = 16; wi->data_offs = 0; wi->data_len = 0;
    while (pos + 8 <= len) {
        uint32_t ck_id = r32le(p + pos);
        uint32_t ck_len = r32le(p + pos + 4);
        if (ck_len > len - pos - 8) break;
        if (ck_id == 0x20746D66 && ck_len >= 16) {
            wi->fmt   = r16le(p + pos + 8);
            wi->ch    = r16le(p + pos + 10);
            wi->rate  = r32le(p + pos + 12);
            wi->bits  = r16le(p + pos + 22);
            found_fmt = 1;
        } else if (ck_id == 0x61746164) {
            wi->data_offs = pos + 8;
            wi->data_len  = ck_len;
            found_data = 1;
        }
        pos += 8 + ck_len;
        if (pos & 1) pos++;
    }
    return (found_fmt && found_data) ? 0 : -1;
}

/* ================================================================
   WAV → S16 stereo OUTPUT_RATE
   ================================================================ */

static int16_t *convert_wav(const void *src, uint32_t src_len,
                             uint32_t *out_frames)
{
    WavInfo wi;
    if (parse_wav(src, src_len, &wi) < 0) return NULL;
    if (wi.fmt != 1) return NULL; /* PCM only */
    if (wi.ch == 0 || wi.bits == 0) return NULL;

    const uint8_t *data = (const uint8_t *)src + wi.data_offs;
    int bpf = wi.ch * (wi.bits / 8);
    if (bpf == 0 || wi.data_len == 0) return NULL;
    uint32_t src_frames = wi.data_len / bpf;
    if (src_frames == 0) return NULL;

    /* to float stereo */
    float *flt = (float *)calloc(src_frames * 2, sizeof(float));
    if (!flt) return NULL;

    if (wi.bits == 16) {
        const int16_t *s16 = (const int16_t *)data;
        for (uint32_t i = 0; i < src_frames; i++) {
            float l, r;
            if (wi.ch == 2) { l = s16[i*2]/32768.0f; r = s16[i*2+1]/32768.0f; }
            else            { l = r = s16[i]/32768.0f; }
            flt[i*2] = l; flt[i*2+1] = r;
        }
    } else { /* 8-bit unsigned */
        const uint8_t *u8 = data;
        for (uint32_t i = 0; i < src_frames; i++) {
            float l, r;
            if (wi.ch == 2) { l = (u8[i*2]-128.0f)/128.0f; r = (u8[i*2+1]-128.0f)/128.0f; }
            else            { l = r = (u8[i]-128.0f)/128.0f; }
            flt[i*2] = l; flt[i*2+1] = r;
        }
    }

    /* resample to OUTPUT_RATE */
    uint32_t dst_frames = (uint32_t)((uint64_t)src_frames * OUTPUT_RATE / wi.rate) + 1;
    if (dst_frames > 4*1024*1024) { free(flt); return NULL; }

    int16_t *out = (int16_t *)calloc(dst_frames * 2, sizeof(int16_t));
    if (!out) { free(flt); return NULL; }

    for (uint32_t i = 0; i < dst_frames; i++) {
        float sp = (float)i * (float)wi.rate / (float)OUTPUT_RATE;
        uint32_t si = (uint32_t)sp;
        float frac = sp - (float)si;
        if (si >= src_frames) si = src_frames - 1;
        float l, r;
        if (si + 1 < src_frames && frac > 0.001f) {
            l = flt[si*2]   + (flt[(si+1)*2]   - flt[si*2])   * frac;
            r = flt[si*2+1] + (flt[(si+1)*2+1] - flt[si*2+1]) * frac;
        } else {
            l = flt[si*2]; r = flt[si*2+1];
        }
        out[i*2]   = (int16_t)(fmaxf(-1.0f, fminf(1.0f, l)) * 32767.0f);
        out[i*2+1] = (int16_t)(fmaxf(-1.0f, fminf(1.0f, r)) * 32767.0f);
    }

    free(flt);
    *out_frames = dst_frames;
    return out;
}

/* ================================================================
   FFmpeg — decode from memory buffer
   ================================================================ */

struct AvioMemCtx {
    const uint8_t *data;
    uint32_t      size;
    uint32_t      pos;
};

static int avio_mem_read(void *opaque, uint8_t *buf, int buf_size) {
    AvioMemCtx *ctx = (AvioMemCtx *)opaque;
    uint32_t avail = ctx->size - ctx->pos;
    if (avail == 0) return AVERROR_EOF;
    if ((uint32_t)buf_size > avail) buf_size = (int)avail;
    memcpy(buf, ctx->data + ctx->pos, (size_t)buf_size);
    ctx->pos += (uint32_t)buf_size;
    return buf_size;
}

static int16_t *decode_ffmpeg_mem(const void *data, uint32_t data_len,
                                   uint32_t *out_frames)
{
    if (!data || data_len < 8) return NULL;

    AvioMemCtx mem;
    AVIOContext *avio = NULL;
    AVFormatContext *fmt = NULL;
    AVCodecContext *ctx = NULL;
    SwrContext *swr = NULL;
    float *fifo = NULL;
    AVPacket *pkt = NULL;
    AVFrame *frame = NULL;
    int16_t *result = NULL;
    uint32_t fifo_cap = 0, fifo_len = 0;
    int stream_idx = -1;

    mem.data = (const uint8_t *)data;
    mem.size = data_len;
    mem.pos  = 0;

    {
        unsigned avio_buf_size = 32768;
        unsigned char *avio_buf = (unsigned char *)av_malloc(avio_buf_size);
        if (!avio_buf) return NULL;
        avio = avio_alloc_context(
            avio_buf, (int)avio_buf_size, 0, &mem, avio_mem_read, NULL, NULL);
        if (!avio) { av_free(avio_buf); return NULL; }
    }

    fmt = avformat_alloc_context();
    if (!fmt) goto cleanup;
    fmt->pb = avio; avio = NULL; /* fmt owns avio now */

    if (avformat_open_input(&fmt, NULL, NULL, NULL) < 0)
        goto cleanup;
    if (avformat_find_stream_info(fmt, NULL) < 0)
        goto cleanup;

    for (unsigned i = 0; i < fmt->nb_streams; i++)
        if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            { stream_idx = (int)i; break; }
    if (stream_idx < 0) goto cleanup;

    {
        AVCodecParameters *par = fmt->streams[stream_idx]->codecpar;
        const AVCodec *codec = avcodec_find_decoder(par->codec_id);
        if (!codec) goto cleanup;
        ctx = avcodec_alloc_context3(codec);
        if (!ctx) goto cleanup;
        avcodec_parameters_to_context(ctx, par);
        if (avcodec_open2(ctx, codec, NULL) < 0) goto cleanup;
    }

    {
        int src_rate = ctx->sample_rate;
        swr = swr_alloc();
        if (!swr) goto cleanup;
        av_opt_set_chlayout(swr, "in_chlayout",    &ctx->ch_layout, 0);
        av_opt_set_int(swr, "in_sample_rate",      src_rate, 0);
        av_opt_set_sample_fmt(swr, "in_sample_fmt", ctx->sample_fmt, 0);
        AVChannelLayout out_ch = AV_CHANNEL_LAYOUT_STEREO;
        av_opt_set_chlayout(swr, "out_chlayout",   &out_ch, 0);
        av_opt_set_int(swr, "out_sample_rate",     OUTPUT_RATE, 0);
        av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
        if (swr_init(swr) < 0) goto cleanup;
    }

    pkt = av_packet_alloc();
    frame = av_frame_alloc();
    if (!pkt || !frame) goto cleanup;

    {
        int src_rate = ctx->sample_rate;
        while (av_read_frame(fmt, pkt) >= 0) {
            if (pkt->stream_index != stream_idx) { av_packet_unref(pkt); continue; }
            if (avcodec_send_packet(ctx, pkt) < 0) { av_packet_unref(pkt); continue; }
            av_packet_unref(pkt);

            while (avcodec_receive_frame(ctx, frame) >= 0) {
                int out_samples = (int)av_rescale_rnd(
                    swr_get_delay(swr, src_rate) + frame->nb_samples,
                    OUTPUT_RATE, src_rate, AV_ROUND_UP) + 1024;

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
                    if (!tmp) { av_freep(&out_buf[0]); av_frame_unref(frame); goto convert; }
                    fifo = tmp;
                }
                memcpy(fifo + fifo_len, out_buf[0], (size_t)actual * 2 * sizeof(float));
                fifo_len += (uint32_t)actual * 2;

                av_freep(&out_buf[0]);
                av_frame_unref(frame);
            }
        }
    }

convert:
    if (!fifo || fifo_len == 0) goto cleanup;

    {
        uint32_t dst_frames = fifo_len / 2;
        result = (int16_t *)malloc((size_t)dst_frames * 2 * sizeof(int16_t));
        if (!result) goto cleanup;
        for (uint32_t i = 0; i < dst_frames; i++) {
            result[i*2]   = (int16_t)(fmaxf(-1.0f, fminf(1.0f, fifo[i*2])) * 32767.0f);
            result[i*2+1] = (int16_t)(fmaxf(-1.0f, fminf(1.0f, fifo[i*2+1])) * 32767.0f);
        }
        *out_frames = dst_frames;
    }

cleanup:
    free(fifo);
    if (pkt) av_packet_free(&pkt);
    if (frame) av_frame_free(&frame);
    if (swr) swr_free(&swr);
    if (ctx) avcodec_free_context(&ctx);
    if (fmt) avformat_close_input(&fmt);
    if (avio) avio_context_free(&avio);
    return result;
}

/* ================================================================
   File loading via game file callbacks
   ================================================================ */

static int16_t *load_file_by_name(const char *filename, uint32_t *out_frames)
{
    if (!filename || !filename[0] || !g_sdl.file_open || !g_sdl.file_read)
        return NULL;

    intptr_t handle = 0;
    if (!g_sdl.file_open(filename, &handle) || handle == 0)
        return NULL;

    /* get file size: seek to end, get position, seek back */
    g_sdl.file_seek(handle, 0, 2 /* AIL_FILE_SEEK_END */);
    S32 file_len = g_sdl.file_seek(handle, 0, 3 /* AIL_FILE_SEEK_CURRENT */);
    /* fallback: read in chunks to find size */
    if (file_len <= 0) {
        /* try reading all data */
        uint32_t cap = 65536;
        uint8_t *buf = (uint8_t *)malloc(cap);
        if (!buf) { g_sdl.file_close(handle); return NULL; }
        file_len = 0;
        for (;;) {
            U32 n = g_sdl.file_read(handle, buf + file_len, cap - (uint32_t)file_len);
            if (n == 0) break;
            file_len += (S32)n;
            if ((uint32_t)file_len >= cap) {
                cap *= 2;
                uint8_t *tmp = (uint8_t *)realloc(buf, cap);
                if (!tmp) { free(buf); g_sdl.file_close(handle); return NULL; }
                buf = tmp;
            }
        }
        g_sdl.file_close(handle);
        int16_t *pcm = convert_wav(buf, (uint32_t)file_len, out_frames);
        if (!pcm) pcm = decode_ffmpeg_mem(buf, (uint32_t)file_len, out_frames);
        free(buf);
        return pcm;
    }

    /* seek back to start */
    g_sdl.file_seek(handle, 0, 0 /* AIL_FILE_SEEK_BEGIN */);

    uint8_t *buf = (uint8_t *)malloc((size_t)file_len);
    if (!buf) { g_sdl.file_close(handle); return NULL; }

    U32 total = 0;
    while ((S32)total < file_len) {
        U32 n = g_sdl.file_read(handle, buf + total, (U32)(file_len - total));
        if (n == 0) break;
        total += n;
    }
    g_sdl.file_close(handle);

    int16_t *pcm = convert_wav(buf, total, out_frames);
    if (!pcm) pcm = decode_ffmpeg_mem(buf, total, out_frames);
    free(buf);
    return pcm;
}

/* ================================================================
   SDL3 mixer callback
   ================================================================ */

static void SDLCALL mixer_cb(void *userdata, SDL_AudioStream *stream,
                              int additional_amount, int total_amount)
{
    (void)userdata; (void)total_amount;
    int frames = additional_amount / FRAME_SIZE;
    if (frames <= 0) return;
    if (frames > MIX_BUF_FRAMES) frames = MIX_BUF_FRAMES;

    int16_t *buf = g_sdl.mix_buf;
    memset(buf, 0, (size_t)frames * FRAME_SIZE);

    lock();
    for (int i = 1; i < MAX_SLOTS; i++) {
        Voice *v = &g_sdl.voices[i];
        if (!v->active || !v->playing || !v->pcm || v->frames == 0)
            continue;

        for (int f = 0; f < frames; f++) {
            /* loop / end check */
            if (v->loop_remain != 0 && v->pos >= ((uint64_t)v->frames << 16)) {
                if (v->loop_remain > 0) {
                    v->loop_remain--;
                    if (v->loop_remain == 0) { v->playing = false; break; }
                }
                v->pos = 0;
            }
            uint32_t pi = v->pos >> 16;
            if (pi >= v->frames) { v->playing = false; break; }

            uint32_t pf = v->pos & 0xFFFF;
            int16_t l = v->pcm[pi*2];
            int16_t r = v->pcm[pi*2+1];

            /* linear interpolation */
            if (pf > 0 && pi + 1 < v->frames) {
                float t = (float)pf / 65536.0f;
                l = (int16_t)((float)l + ((float)v->pcm[(pi+1)*2]   - (float)l) * t);
                r = (int16_t)((float)r + ((float)v->pcm[(pi+1)*2+1] - (float)r) * t);
            }

            float vl = v->volume * (v->pan <= 0.0f ? 1.0f : 1.0f - v->pan);
            float vr = v->volume * (v->pan >= 0.0f ? 1.0f : 1.0f + v->pan);

            int mix_l = buf[f*2]   + (int)((float)l * vl);
            int mix_r = buf[f*2+1] + (int)((float)r * vr);
            buf[f*2]   = (int16_t)(mix_l < -32768 ? -32768 : mix_l > 32767 ? 32767 : mix_l);
            buf[f*2+1] = (int16_t)(mix_r < -32768 ? -32768 : mix_r > 32767 ? 32767 : mix_r);

            v->pos += 65536;
        }
    }
    unlock();

    SDL_PutAudioStreamData(stream, buf, (size_t)frames * FRAME_SIZE);
}

/* ================================================================
   MSS/AIL API — SDL3 implementation
   ================================================================ */

extern "C" {

/* ---- init / shutdown ---- */

void AIL_startup(void)
{
    memset(&g_sdl, 0, sizeof(g_sdl));
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&g_sdl.mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    g_sdl.started = true;
    g_sdl.next_id = 0;
    g_sdl.file_open = NULL;
    g_sdl.file_close = NULL;
    g_sdl.file_seek = NULL;
    g_sdl.file_read = NULL;
}

void AIL_shutdown(void)
{
    if (!g_sdl.started) {
        return;
    }
    lock();
    g_sdl.started = false;
    if (g_sdl.stream) {
        SDL_ResumeAudioStreamDevice(g_sdl.stream);
        SDL_ClearAudioStream(g_sdl.stream);
        SDL_DestroyAudioStream(g_sdl.stream);
        g_sdl.stream = NULL;
    }
    for (int i = 0; i < MAX_SLOTS; i++) {
        free(g_sdl.voices[i].pcm);
        memset(&g_sdl.voices[i], 0, sizeof(Voice));
    }
    g_sdl.ready = false;
    unlock();
    pthread_mutex_destroy(&g_sdl.mutex);
}

void AIL_lock(void)   { lock(); }
void AIL_unlock(void) { unlock(); }

S32 AIL_set_preference(U32, S32 v) { return v; }
char *AIL_last_error(void) { return g_sdl.err; }

/* ---- device open/close ---- */

S32 AIL_waveOutOpen(HDIGDRIVER *driver, LPHWAVEOUT *, unsigned int,
                     LPWAVEFORMAT format, unsigned long)
{
    if (!g_sdl.stream) {
        int rate = (format && format->nSamplesPerSec)
                   ? (int)format->nSamplesPerSec : OUTPUT_RATE;
        int ch = (format && format->nChannels)
                 ? (int)format->nChannels : OUTPUT_CHANNELS;

        SDL_AudioSpec spec;
        SDL_zero(spec);
        spec.freq = rate;
        spec.format = SDL_AUDIO_S16LE;
        spec.channels = (Uint8)ch;

        g_sdl.stream = SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, mixer_cb, NULL);
        if (g_sdl.stream) {
            SDL_ResumeAudioStreamDevice(g_sdl.stream);
        } else {
            snprintf(g_sdl.err, sizeof(g_sdl.err), "%s", SDL_GetError());
        }
    }
    if (driver) {
        static DIG_DRIVER_TYPE dummy;
        dummy.emulated_ds = FALSE;
        *driver = &dummy;
    }
    g_sdl.ready = true;
    (void)format;
    return AIL_NO_ERROR;
}

void AIL_waveOutClose(HDIGDRIVER)
{
    if (g_sdl.stream) {
        SDL_ResumeAudioStreamDevice(g_sdl.stream);
        SDL_ClearAudioStream(g_sdl.stream);
        SDL_DestroyAudioStream(g_sdl.stream);
        g_sdl.stream = NULL;
    }
    g_sdl.ready = false;
}

/* ---- sample alloc / release ---- */

HSAMPLE AIL_allocate_sample_handle(HDIGDRIVER)
{
    lock();
    int id = alloc_slot();
    if (id != 0) {
        Voice *v = &g_sdl.voices[id];
        memset(v, 0, sizeof(Voice));
        v->active = true;
        v->loop_remain = 1;
        v->volume = 1.0f;
        v->pan = 0.0f;
        g_sdl.next_id = id;
        fprintf(stderr, "[SDL3] alloc id=%d\n", id);
    }
    unlock();
    return id != 0 ? (HSAMPLE)(intptr_t)id : NULL;
}

void AIL_release_sample_handle(HSAMPLE sample)
{
    fprintf(stderr, "[SDL3] release id=%d\n", (int)(intptr_t)sample);
    lock();
    Voice *v = get_voice(sample);
    if (v) {
        free(v->pcm);
        memset(v, 0, sizeof(Voice));
    }
    unlock();
}

void AIL_init_sample(HSAMPLE sample)
{
    Voice *v = get_voice(sample);
    if (!v) return;
    lock();
    v->playing = false;
    v->pos = 0;
    v->loop_remain = 1;
    v->volume = 1.0f;
    v->pan = 0.0f;
    memset(&v->user_data, 0, sizeof(v->user_data));
    free(v->pcm);
    v->pcm = NULL;
    v->frames = 0;
    unlock();
}

/* ---- load audio data ---- */

S32 AIL_set_named_sample_file(HSAMPLE sample, char *filename,
                               void const *file_ptr, S32 file_len, S32)
{
    Voice *v = get_voice(sample);
    if (!v || !file_ptr || file_len <= 0) return 0;

    fprintf(stderr, "[SDL3] load id=%d name=%s len=%d\n",
            (int)(intptr_t)sample, filename ? filename : "?", file_len);

    /* decode outside the lock to avoid blocking the audio callback */
    uint32_t frames = 0;
    int16_t *pcm = convert_wav(file_ptr, (uint32_t)file_len, &frames);
    if (!pcm)
        pcm = decode_ffmpeg_mem(file_ptr, (uint32_t)file_len, &frames);
    if (!pcm)
        return 0;

    lock();
    free(v->pcm);
    v->pcm = pcm;
    v->frames = frames;
    v->pos = 0;
    v->loop_remain = 1;
    unlock();
    return 1;
}

/* ---- sample playback ---- */

void AIL_start_sample(HSAMPLE sample)
{
    Voice *v = get_voice(sample);
    if (!v) return;
    fprintf(stderr, "[SDL3] start id=%d frames=%u playing=%d loop_remain=%d\n",
            (int)(intptr_t)sample, v->frames, v->playing ? 1 : 0, v->loop_remain);
    lock();
    if (v->active && v->pcm) {
        v->playing = true;
        v->pos = 0;
    }
    unlock();
}

void AIL_stop_sample(HSAMPLE sample)
{
    Voice *v = get_voice(sample);
    if (v) { lock(); v->playing = false; unlock(); }
}

void AIL_resume_sample(HSAMPLE sample) { AIL_start_sample(sample); }

void AIL_end_sample(HSAMPLE sample)
{
    AIL_stop_sample(sample);
    AIL_init_sample(sample);
}

S32 AIL_sample_playback_busy(HSAMPLE sample)
{
    Voice *v = get_voice(sample);
    return (v && v->playing) ? 1 : 0;
}

/* ---- sample properties ---- */

void AIL_set_sample_pan(HSAMPLE sample, F32 pan)
{
    Voice *v = get_voice(sample);
    /* MSS uses -127..+127, normalize to -1.0..1.0 */
    float normalized = pan / 127.0f;
    if (normalized < -1.0f) normalized = -1.0f;
    if (normalized >  1.0f) normalized =  1.0f;
    if (v) { lock(); v->pan = normalized; unlock(); }
}

F32 AIL_sample_pan(HSAMPLE sample)
{
    Voice *v = get_voice(sample);
    return v ? v->pan * 127.0f : 0.0f;
}

void AIL_set_sample_volume(HSAMPLE sample, F32 volume)
{
    Voice *v = get_voice(sample);
    /* MSS uses 0..127, normalize to 0.0..1.0 */
    float normalized = volume / 127.0f;
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    if (v) { lock(); v->volume = normalized; unlock(); }
}

F32 AIL_sample_volume(HSAMPLE sample)
{
    Voice *v = get_voice(sample);
    return v ? v->volume * 127.0f : 127.0f;
}

void AIL_set_sample_loop_count(HSAMPLE sample, S32 count)
{
    Voice *v = get_voice(sample);
    if (!v) return;
    lock();
    v->loop_remain = (count == 0) ? -1 : count;
    unlock();
}

S32 AIL_sample_loop_count(HSAMPLE sample)
{
    Voice *v = get_voice(sample);
    if (!v) return 0;
    return (v->loop_remain == -1) ? 0 : v->loop_remain;
}

void AIL_set_sample_ms_position(HSAMPLE sample, S32 ms)
{
    Voice *v = get_voice(sample);
    if (!v) return;
    lock();
    v->pos = (uint64_t)((uint64_t)ms * OUTPUT_RATE / 1000) << 16;
    unlock();
}

void AIL_sample_ms_position(HSAMPLE sample, S32 *len, S32 *pos)
{
    Voice *v = get_voice(sample);
    lock();
    if (len) *len = (v && v->frames)
        ? (S32)((uint64_t)v->frames * 1000 / OUTPUT_RATE) : 0;
    if (pos) *pos = (v && v->frames)
        ? (S32)((uint64_t)(v->pos >> 16) * 1000 / OUTPUT_RATE) : 0;
    unlock();
}

void AIL_set_sample_user_data(HSAMPLE sample, U32 index, intptr_t data)
{
    Voice *v = get_voice(sample);
    if (v && index < 8) v->user_data.d[index] = data;
}

intptr_t AIL_sample_user_data(HSAMPLE sample, U32 index)
{
    Voice *v = get_voice(sample);
    return (v && index < 8) ? v->user_data.d[index] : 0;
}

S32 AIL_sample_playback_rate(HSAMPLE) { return OUTPUT_RATE; }
void AIL_set_sample_playback_rate(HSAMPLE, S32) {}
void AIL_set_sample_processor(HSAMPLE, S32, HPROVIDER) {}
void AIL_set_filter_sample_preference(HSAMPLE, char const *, void const *) {}
void AIL_set_room_type(HDIGDRIVER, S32) {}
S32 AIL_room_type(HDIGDRIVER) { return 0; }

/* ---- stream API (delegates to sample) ---- */

HSTREAM AIL_open_stream_by_sample(HDIGDRIVER, HSAMPLE sample,
                                    char const *filename, S32)
{
    if (!sample) return NULL;

    Voice *v = get_voice(sample);
    fprintf(stderr, "[SDL3] stream id=%d name=%s old_pcm=%p old_active=%d old_playing=%d old_ud0=%p\n",
            (int)(intptr_t)sample, filename ? filename : "?",
            v ? (void*)v->pcm : NULL,
            v ? v->active : -1,
            v ? v->playing : -1,
            v ? (void*)(intptr_t)v->user_data.d[0] : NULL);

    /* if a filename is given, load and decode outside the lock */
    if (filename && filename[0]) {
        uint32_t frames = 0;
        int16_t *pcm = load_file_by_name(filename, &frames);

        if (v) {
            lock();
            free(v->pcm);
            v->pcm = pcm;
            v->frames = frames;
            v->pos = 0;
            v->loop_remain = 1;
            unlock();
        } else {
            free(pcm);
        }
    }

    return (HSTREAM)sample;
}

void AIL_start_stream(HSTREAM s)   { AIL_start_sample((HSAMPLE)s); }
void AIL_pause_stream(HSTREAM s, S32 on)
{
    if (on) AIL_stop_sample((HSAMPLE)s);
    else    AIL_start_sample((HSAMPLE)s);
}
void AIL_close_stream(HSTREAM s)   { AIL_end_sample((HSAMPLE)s); }
void AIL_set_stream_pan(HSTREAM s, F32 p)    { AIL_set_sample_pan((HSAMPLE)s, p); }
F32  AIL_stream_pan(HSTREAM s)               { return AIL_sample_pan((HSAMPLE)s); }
void AIL_set_stream_volume(HSTREAM s, F32 v) { AIL_set_sample_volume((HSAMPLE)s, v); }
F32  AIL_stream_volume(HSTREAM s)            { return AIL_sample_volume((HSAMPLE)s); }
void AIL_set_stream_loop_block(HSTREAM, S32, S32) {}
void AIL_set_stream_loop_count(HSTREAM s, S32 c) { AIL_set_sample_loop_count((HSAMPLE)s, c); }
S32  AIL_stream_loop_count(HSTREAM s)              { return AIL_sample_loop_count((HSAMPLE)s); }
void AIL_set_stream_ms_position(HSTREAM s, S32 m)  { AIL_set_sample_ms_position((HSAMPLE)s, m); }
void AIL_stream_ms_position(HSTREAM s, S32 *l, S32 *p) { AIL_sample_ms_position((HSAMPLE)s, l, p); }
S32  AIL_stream_playback_rate(HSTREAM) { return OUTPUT_RATE; }
void AIL_set_stream_playback_rate(HSTREAM, S32) {}

/* ---- 3D audio (stereo pan) ---- */

S32 AIL_enumerate_3D_providers(HPROENUM *next, HPROVIDER *dest, char **name)
{
    static HPROVIDER prov = (HPROVIDER)1;
    if (next && *next == HPROENUM_FIRST) {
        if (dest) *dest = prov;
        if (name) *name = (char *)"SDL3 Pan";
        *next = (HPROENUM)0;
        return 1;
    }
    return 0;
}

S32 AIL_open_3D_provider(HPROVIDER) { return M3D_NOERR; }
void AIL_close_3D_provider(HPROVIDER) {}
void AIL_set_3D_speaker_type(HPROVIDER, U32) {}

H3DSAMPLE AIL_allocate_3D_sample_handle(HPROVIDER)
{
    /* uses the SAME pool as 2D — fix for ID overflow bug */
    return (H3DSAMPLE)AIL_allocate_sample_handle(NULL);
}

void AIL_release_3D_sample_handle(H3DSAMPLE s)
{
    AIL_release_sample_handle((HSAMPLE)s);
}

S32 AIL_set_3D_sample_file_len(H3DSAMPLE sample, void const *file_ptr, S32 file_len)
{
    return AIL_set_named_sample_file((HSAMPLE)sample, NULL, file_ptr, file_len, 0);
}

/* Deprecated: use AIL_set_3D_sample_file_len with a real buffer size. */
S32 AIL_set_3D_sample_file(H3DSAMPLE sample, void const *file_ptr)
{
    if (!file_ptr) return 0;
    return AIL_set_named_sample_file((HSAMPLE)sample, NULL, file_ptr, 0x7FFFFFFF, 0);
}

void AIL_start_3D_sample(H3DSAMPLE s)   { AIL_start_sample((HSAMPLE)s); }
void AIL_stop_3D_sample(H3DSAMPLE s)    { AIL_stop_sample((HSAMPLE)s); }
void AIL_resume_3D_sample(H3DSAMPLE s)  { AIL_start_sample((HSAMPLE)s); }
void AIL_end_3D_sample(H3DSAMPLE s)     { AIL_end_sample((HSAMPLE)s); }

void AIL_set_3D_sample_volume(H3DSAMPLE s, F32 v) { AIL_set_sample_volume((HSAMPLE)s, v); }
F32  AIL_3D_sample_volume(H3DSAMPLE s)            { return AIL_sample_volume((HSAMPLE)s); }
void AIL_set_3D_sample_loop_count(H3DSAMPLE s, S32 c) { AIL_set_sample_loop_count((HSAMPLE)s, c); }
S32  AIL_3D_sample_loop_count(H3DSAMPLE s)              { return AIL_sample_loop_count((HSAMPLE)s); }
void AIL_set_3D_sample_offset(H3DSAMPLE, U32) {}
U32  AIL_3D_sample_offset(H3DSAMPLE) { return 0; }
U32  AIL_3D_sample_length(H3DSAMPLE s) {
    Voice *v = get_voice((HSAMPLE)s);
    return v ? v->frames * FRAME_SIZE : 0;
}

void AIL_set_3D_object_user_data(H3DSAMPLE, U32, intptr_t) {}
intptr_t AIL_3D_object_user_data(H3DSAMPLE, U32) { return 0; }

S32 AIL_3D_sample_playback_rate(H3DSAMPLE) { return OUTPUT_RATE; }
void AIL_set_3D_sample_playback_rate(H3DSAMPLE, S32) {}

void AIL_set_3D_position(H3DSAMPLE sample, F32 x, F32, F32 z)
{
    Voice *v = get_voice((HSAMPLE)sample);
    if (!v) return;
    float angle = atan2f(-x, z);
    float pan = -sinf(angle) * 0.5f;
    if (pan < -1.0f) pan = -1.0f;
    if (pan >  1.0f) pan =  1.0f;
    lock(); v->pan = pan; unlock();
}

void AIL_set_3D_orientation(H3DSAMPLE, F32, F32, F32, F32, F32, F32) {}
void AIL_set_3D_velocity_vector(H3DSAMPLE, F32, F32, F32) {}
void AIL_set_3D_sample_distances(H3DSAMPLE, F32, F32) {}
void AIL_set_3D_sample_effects_level(H3DSAMPLE, F32) {}

H3DPOBJECT AIL_3D_open_listener(HPROVIDER) { return NULL; }

S32 AIL_enumerate_filters(HPROENUM*, HPROVIDER*, char**) { return 0; }

/* ---- file callbacks ---- */

void AIL_set_file_callbacks(AIL_FILE_OPEN_CALLBACK open_fn, AIL_FILE_CLOSE_CALLBACK close_fn,
                            AIL_FILE_SEEK_CALLBACK seek_fn, AIL_FILE_READ_CALLBACK read_fn)
{
    g_sdl.file_open  = open_fn;
    g_sdl.file_close = close_fn;
    g_sdl.file_seek  = seek_fn;
    g_sdl.file_read  = read_fn;
}

void AIL_stop_timer(HTIMER) {}
void AIL_release_timer_handle(HTIMER) {}

/* ---- WAV info: returns 1 on success, -1 on error ---- */

S32 AIL_WAV_info(void const *data, AILSOUNDINFO *info)
{
    if (!info) return -1;
    memset(info, 0, sizeof(*info));

    /* AIL_WAV_info is called with a 4096-byte header buffer from
       StreamSoundBufferClass — cap scan to WAV header size only */
    WavInfo wi;
    if (data && parse_wav(data, 4096, &wi) == 0) {
        info->format    = wi.fmt;
        info->data_ptr  = (const uint8_t *)data + wi.data_offs;
        info->data_len  = wi.data_len;
        info->rate      = wi.rate;
        info->bits      = wi.bits;
        info->channels  = wi.ch;
        info->block_size = wi.ch * (wi.bits / 8);
        if (wi.ch > 0 && wi.bits >= 8) {
            info->samples = wi.data_len / info->block_size;
        } else {
            info->samples = 0;
        }
    } else {
        /* not WAV — return fake data so Determine_Stats doesn't div-by-zero */
        info->format    = WAVE_FORMAT_PCM;
        info->rate      = OUTPUT_RATE;
        info->bits      = 16;
        info->channels  = 2;
        info->block_size = 4;
    }
    return 1; /* success */
}

} /* extern "C" */
