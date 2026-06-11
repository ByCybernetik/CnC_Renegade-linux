/*
 * Bink (.bik) playback via FFmpeg + SDL3 audio.
 */

#include "Bink.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <SDL3/SDL.h>

#include "bufffile.h"
#include "ffactory.h"
#include "wwfile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>


struct BinkMemIO {
	const unsigned char *data;
	unsigned long size;
	unsigned long pos;
};

struct BinkFile : public BINK {
	AVFormatContext *fmt;
	AVCodecContext *vdec;
	AVCodecContext *adec;
	SwsContext *sws;
	SwrContext *swr;
	AVPacket *pkt;
	AVFrame *vframe;
	AVFrame *aframe;
	int vstream;
	int astream;
	int audio_out_channels;

	std::vector<unsigned char> file_data;
	BinkMemIO mem_io;
	std::vector<unsigned char> rgb565;
	int decoded_frames;
	bool video_eof;
	bool frame_ready;

	SDL_AudioStream *audio_stream;
	unsigned long audio_pcm_bytes;
	int audio_sample_rate;
	int audio_channels;
	unsigned long ms_per_frame;
	unsigned long start_ms;
	unsigned long last_frame_ms;
	bool playback_started;
};

static int bink_mem_read(void *opaque, uint8_t *buf, int buf_size)
{
	BinkMemIO *io = (BinkMemIO *)opaque;
	if (io == NULL || buf == NULL || buf_size <= 0 || io->pos >= io->size) {
		return AVERROR_EOF;
	}
	const unsigned long remain = io->size - io->pos;
	const int to_read = (int)((remain < (unsigned long)buf_size) ? remain : (unsigned long)buf_size);
	memcpy(buf, io->data + io->pos, (size_t)to_read);
	io->pos += (unsigned long)to_read;
	return to_read;
}

static int64_t bink_mem_seek(void *opaque, int64_t offset, int whence)
{
	BinkMemIO *io = (BinkMemIO *)opaque;
	if (io == NULL) {
		return AVERROR(EINVAL);
	}
	switch (whence) {
	case AVSEEK_SIZE:
		return (int64_t)io->size;
	case SEEK_SET:
		io->pos = (offset < 0) ? 0u : (unsigned long)offset;
		break;
	case SEEK_CUR:
		io->pos = (unsigned long)((int64_t)io->pos + offset);
		break;
	case SEEK_END:
		io->pos = (unsigned long)((int64_t)io->size + offset);
		break;
	default:
		return AVERROR(EINVAL);
	}
	if (io->pos > io->size) {
		io->pos = io->size;
	}
	return (int64_t)io->pos;
}

static void bink_normalize_path(char *out, size_t out_cap, const char *in)
{
	size_t i;
	size_t n = 0;

	if (out == NULL || out_cap == 0 || in == NULL) {
		return;
	}
	for (i = 0; in[i] != '\0' && n + 1 < out_cap; i++) {
		out[n++] = (in[i] == '\\') ? '/' : in[i];
	}
	out[n] = '\0';
}

static int bink_load_buffered(const char *path, std::vector<unsigned char> &out, const char **resolved_name)
{
	BufferedFileClass file;
	int size;
	int got;

	if (path == NULL || resolved_name == NULL) {
		return 0;
	}
	*resolved_name = path;

	if (!file.Open(path, FileClass::READ)) {
		return 0;
	}
	{
		const char *opened = file.File_Name();
		if (opened != NULL && opened[0] != '\0') {
			*resolved_name = opened;
		}
	}
	size = file.Size();
	if (size <= 0) {
		file.Close();
		return 0;
	}
	out.resize((size_t)size);
	got = file.Read(out.data(), size);
	file.Close();
	return got == size;
}

static int bink_load_stdio(const char *path, std::vector<unsigned char> &out)
{
	FILE *f;
	long sz;
	size_t rd;

	f = fopen(path, "rb");
	if (f == NULL) {
		return 0;
	}
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return 0;
	}
	sz = ftell(f);
	if (sz <= 0) {
		fclose(f);
		return 0;
	}
	if (fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return 0;
	}
	out.resize((size_t)sz);
	rd = fread(out.data(), 1, (size_t)sz, f);
	fclose(f);
	return rd == (size_t)sz;
}

static int bink_load_factory(const char *path, std::vector<unsigned char> &out)
{
	FileFactoryClass *factory;
	FileClass *file;
	int size;
	int got;

	if (path == NULL) {
		return 0;
	}
	factory = _TheFileFactory;
	if (factory == NULL) {
		return 0;
	}
	file = factory->Get_File(path);
	if (file == NULL) {
		return 0;
	}
	if (!file->Is_Available()) {
		factory->Return_File(file);
		return 0;
	}
	if (!file->Is_Open() && file->Open() != 0) {
		factory->Return_File(file);
		return 0;
	}
	size = file->Size();
	if (size <= 0) {
		if (file->Is_Open()) {
			file->Close();
		}
		factory->Return_File(file);
		return 0;
	}
	out.resize((size_t)size);
	got = file->Read(out.data(), size);
	if (file->Is_Open()) {
		file->Close();
	}
	factory->Return_File(file);
	return got == size;
}

static int bink_load_bytes(const char *name, std::vector<unsigned char> &out)
{
	char path[1024];
	char movies_path[1024];
	const char *resolved_name = path;
	const char *basename;

	if (name == NULL || name[0] == '\0') {
		return 0;
	}
	bink_normalize_path(path, sizeof(path), name);
	basename = strrchr(path, '/');
	basename = (basename != NULL) ? (basename + 1) : path;

	if (bink_load_factory(name, out)) {
		return 1;
	}
	if (bink_load_factory(path, out)) {
		return 1;
	}
	if (bink_load_buffered(name, out, &resolved_name)) {
		return 1;
	}
	if (bink_load_buffered(path, out, &resolved_name)) {
		return 1;
	}
	if (bink_load_stdio(path, out)) {
		return 1;
	}
#if defined(RENEGADE_LINUX)
	if (basename[0] != '\0') {
		snprintf(movies_path, sizeof(movies_path), "Data\\Movies\\%s", basename);
		if (bink_load_buffered(movies_path, out, &resolved_name)) {
			return 1;
		}
	}
#endif
	return 0;
}

static int bink_open_avformat(BinkFile *bf, const char *name)
{
	AVIOContext *avio = NULL;
	unsigned char *avio_buf;

	if (bf == NULL || name == NULL) {
		return 0;
	}

	if (!bink_load_bytes(name, bf->file_data)) {
		return 0;
	}

	bf->mem_io.data = bf->file_data.data();
	bf->mem_io.size = (unsigned long)bf->file_data.size();
	bf->mem_io.pos = 0;

	avio_buf = (unsigned char *)av_malloc(4096);
	if (avio_buf == NULL) {
		return 0;
	}
	avio = avio_alloc_context(avio_buf, 4096, 0, &bf->mem_io, bink_mem_read, NULL, bink_mem_seek);
	if (avio == NULL) {
		av_free(avio_buf);
		return 0;
	}
	bf->fmt = avformat_alloc_context();
	if (bf->fmt == NULL) {
		avio_context_free(&avio);
		return 0;
	}
	bf->fmt->pb = avio;
	if (avformat_open_input(&bf->fmt, NULL, NULL, NULL) < 0) {
		avformat_close_input(&bf->fmt);
		return 0;
	}
	return 1;
}

static BinkFile *bink_from_handle(HBINK bink)
{
	return (BinkFile *)bink;
}

static void bink_free(BinkFile *bf)
{
	if (bf == NULL) {
		return;
	}
	if (bf->audio_stream != NULL) {
		SDL_DestroyAudioStream(bf->audio_stream);
		bf->audio_stream = NULL;
	}
	if (bf->pkt != NULL) {
		av_packet_free(&bf->pkt);
	}
	if (bf->vframe != NULL) {
		av_frame_free(&bf->vframe);
	}
	if (bf->aframe != NULL) {
		av_frame_free(&bf->aframe);
	}
	if (bf->sws != NULL) {
		sws_freeContext(bf->sws);
	}
	if (bf->swr != NULL) {
		swr_free(&bf->swr);
	}
	if (bf->vdec != NULL) {
		avcodec_free_context(&bf->vdec);
	}
	if (bf->adec != NULL) {
		avcodec_free_context(&bf->adec);
	}
	if (bf->fmt != NULL) {
		avformat_close_input(&bf->fmt);
	}
	delete bf;
}

static int bink_append_audio_pcm(BinkFile *bf, AVFrame *aframe, std::vector<unsigned char> &pcm, int out_ch)
{
	const int out_samples = swr_get_out_samples(bf->swr, aframe->nb_samples);
	const size_t old = pcm.size();
	const size_t need = old + (size_t)out_samples * (size_t)out_ch * 2u;
	uint8_t *out_planes[1];
	int converted;

	pcm.resize(need);
	out_planes[0] = pcm.data() + old;
	converted = swr_convert(
		bf->swr,
		out_planes,
		out_samples,
		(const uint8_t **)aframe->data,
		aframe->nb_samples);
	if (converted > 0) {
		pcm.resize(old + (size_t)converted * (size_t)out_ch * 2u);
	} else {
		pcm.resize(old);
	}
	return converted > 0;
}

static int bink_init_audio_device(BinkFile *bf)
{
	int rate = 44100;
	int out_ch = 2;
	SDL_AudioSpec spec;

	if (bf == NULL || bf->adec == NULL) {
		return 0;
	}

	rate = bf->adec->sample_rate > 0 ? bf->adec->sample_rate : 44100;
	out_ch = (bf->adec->ch_layout.nb_channels == 1) ? 1 : 2;

	SDL_zero(spec);
	spec.freq = rate;
	spec.format = SDL_AUDIO_S16LE;
	spec.channels = (Sint8)out_ch;

	bf->audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
	if (bf->audio_stream == NULL) {
		return 0;
	}

	bf->audio_sample_rate = rate;
	bf->audio_channels = out_ch;
	bf->audio_out_channels = out_ch;
	bf->audio_pcm_bytes = 0;
	return 1;
}

static void bink_begin_playback(BinkFile *bf)
{
	if (bf == NULL || bf->playback_started) {
		return;
	}

	bf->playback_started = true;
	bf->start_ms = (unsigned long)SDL_GetTicks();
	if (bf->audio_stream != NULL) {
		SDL_ResumeAudioStreamDevice(bf->audio_stream);
	}
}

static int bink_queue_audio_frame(BinkFile *bf, AVFrame *aframe)
{
	std::vector<unsigned char> pcm;
	const int out_ch = bf->audio_out_channels > 0 ? bf->audio_out_channels : 2;

	if (bf == NULL || bf->audio_stream == NULL || aframe == NULL || bf->swr == NULL) {
		return 0;
	}
	if (!bink_append_audio_pcm(bf, aframe, pcm, out_ch) || pcm.empty()) {
		return 0;
	}
	if (!SDL_PutAudioStreamData(bf->audio_stream, pcm.data(), (int)pcm.size())) {
		return 0;
	}
	bf->audio_pcm_bytes += (unsigned long)pcm.size();
	return 1;
}

static int bink_decode_audio_packet(BinkFile *bf)
{
	if (bf == NULL || bf->adec == NULL || bf->aframe == NULL || bf->pkt == NULL) {
		return 0;
	}
	if (avcodec_send_packet(bf->adec, bf->pkt) < 0) {
		return 0;
	}

	while (avcodec_receive_frame(bf->adec, bf->aframe) == 0) {
		bink_queue_audio_frame(bf, bf->aframe);
		av_frame_unref(bf->aframe);
	}
	return 1;
}

static unsigned long bink_frame_pts_ms(const BinkFile *bf, unsigned long frame_index)
{
	const AVRational ms_time_base = {1, 1000};

	if (bf == NULL || bf->fmt == NULL || bf->vstream < 0) {
		return frame_index * bf->ms_per_frame;
	}

	return (unsigned long)av_rescale_q(
		(int64_t)frame_index,
		bf->fmt->streams[bf->vstream]->time_base,
		ms_time_base);
}

static unsigned long bink_audio_elapsed_ms(const BinkFile *bf)
{
	const int queued = SDL_GetAudioStreamQueued(bf->audio_stream);
	unsigned long played;
	unsigned long bytes_per_sec;

	if (bf->audio_stream == NULL || bf->audio_pcm_bytes == 0 ||
		bf->audio_sample_rate <= 0 || bf->audio_channels <= 0)
	{
		return 0;
	}
	if (queued < 0) {
		return 0;
	}

	bytes_per_sec =
		(unsigned long)bf->audio_sample_rate * (unsigned long)bf->audio_channels * 2u;
	if (bytes_per_sec == 0) {
		return 0;
	}

	if (!bf->playback_started) {
		return 0;
	}
	if ((unsigned long)queued >= bf->audio_pcm_bytes) {
		played = 0;
	} else {
		played = bf->audio_pcm_bytes - (unsigned long)queued;
	}

	return (unsigned long)(((unsigned long long)played * 1000ULL) / bytes_per_sec);
}

static void bink_pack_rgba_to_rgb565(
	const uint8_t *rgba,
	int src_w,
	int src_h,
	unsigned out_w,
	unsigned out_h,
	std::vector<unsigned char> &rgb565)
{
	const int copy_w = src_w < (int)out_w ? src_w : (int)out_w;
	const int copy_h = src_h < (int)out_h ? src_h : (int)out_h;

	rgb565.resize((size_t)out_w * (size_t)out_h * 2u, 0);
	for (int y = 0; y < copy_h; ++y) {
		for (int x = 0; x < copy_w; ++x) {
			const int ri = (y * src_w + x) * 4;
			const uint8_t r = rgba[(size_t)ri];
			const uint8_t g = rgba[(size_t)ri + 1];
			const uint8_t b = rgba[(size_t)ri + 2];
			const uint16_t px =
				(uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
			const size_t o = ((size_t)y * (size_t)out_w + (size_t)x) * 2u;
			rgb565[o] = (uint8_t)(px & 0xFF);
			rgb565[o + 1] = (uint8_t)((px >> 8) & 0xFF);
		}
	}
}

static int bink_convert_frame_to_rgb565(BinkFile *bf)
{
	if (bf == NULL || bf->vframe == NULL || bf->Width == 0 || bf->Height == 0) {
		return 0;
	}

	const int src_w = bf->vframe->width > 0 ? bf->vframe->width : (int)bf->Width;
	const int src_h = bf->vframe->height > 0 ? bf->vframe->height : (int)bf->Height;
	const AVPixelFormat src_fmt =
		bf->vframe->format != AV_PIX_FMT_NONE
			? (AVPixelFormat)bf->vframe->format
			: bf->vdec->pix_fmt;

	bf->sws = sws_getCachedContext(
		bf->sws,
		src_w,
		src_h,
		src_fmt,
		src_w,
		src_h,
		AV_PIX_FMT_RGBA,
		SWS_BILINEAR,
		NULL,
		NULL,
		NULL);
	if (bf->sws == NULL) {
		av_frame_unref(bf->vframe);
		bf->frame_ready = false;
		return 0;
	}

	{
		const int *coeffs = sws_getCoefficients(SWS_CS_ITU601);
		if (coeffs != NULL) {
			sws_setColorspaceDetails(bf->sws, coeffs, 0, coeffs, 1, 0, 1 << 16, 1 << 16);
		}
	}

	std::vector<uint8_t> rgba((size_t)src_w * (size_t)src_h * 4u);
	uint8_t *dst_data[1] = { rgba.data() };
	int dst_stride[1] = { src_w * 4 };

	const int ret = sws_scale(
		bf->sws,
		(const uint8_t *const *)bf->vframe->data,
		bf->vframe->linesize,
		0,
		src_h,
		dst_data,
		dst_stride);
	av_frame_unref(bf->vframe);

	if (ret <= 0) {
		bf->frame_ready = false;
		return 0;
	}

	bink_pack_rgba_to_rgb565(
		rgba.data(),
		src_w,
		src_h,
		bf->Width,
		bf->Height,
		bf->rgb565);
	bf->frame_ready = true;

	return 1;
}

static int bink_decode_video_frame(BinkFile *bf, int frame_index)
{
	if (bf == NULL || bf->video_eof) {
		return 0;
	}

	while (bf->decoded_frames <= frame_index && !bf->video_eof) {
		int got_frame = 0;

		while (!got_frame) {
			if (av_read_frame(bf->fmt, bf->pkt) < 0) {
				bf->video_eof = true;
				avcodec_send_packet(bf->vdec, NULL);
				if (avcodec_receive_frame(bf->vdec, bf->vframe) == 0) {
					got_frame = 1;
				}
				break;
			}
			if (bf->astream >= 0 && bf->pkt->stream_index == bf->astream) {
				bink_decode_audio_packet(bf);
				av_packet_unref(bf->pkt);
				continue;
			}
			if (bf->pkt->stream_index != bf->vstream) {
				av_packet_unref(bf->pkt);
				continue;
			}
			if (avcodec_send_packet(bf->vdec, bf->pkt) < 0) {
				av_packet_unref(bf->pkt);
				continue;
			}
			av_packet_unref(bf->pkt);
			while (avcodec_receive_frame(bf->vdec, bf->vframe) == 0) {
				got_frame = 1;
				break;
			}
		}

		if (!got_frame) {
			bf->video_eof = true;
			break;
		}

		if (bf->decoded_frames == frame_index) {
			if (bink_convert_frame_to_rgb565(bf)) {
				bf->decoded_frames++;
				return 1;
			}
			return 0;
		}
		av_frame_unref(bf->vframe);
		bf->decoded_frames++;
	}

	return bf->frame_ready && bf->decoded_frames > frame_index;
}

extern "C" {

void BinkSoundUseDirectSound(long)
{
}

HBINK BinkOpen(const char *name, unsigned long)
{
	BinkFile *bf;
	AVCodecParameters *vpar;
	AVCodecParameters *apar;
	const AVCodec *vcodec;
	const AVCodec *acodec;
	AVRational fps;
	double duration_sec;

	bf = new BinkFile();
	bf->Width = 0;
	bf->Height = 0;
	bf->Frames = 0;
	bf->FrameNum = 0;
	bf->FrameRate = 0;
	bf->FrameRateDiv = 0;
	bf->fmt = NULL;
	bf->vdec = NULL;
	bf->adec = NULL;
	bf->sws = NULL;
	bf->swr = NULL;
	bf->pkt = NULL;
	bf->vframe = NULL;
	bf->vstream = -1;
	bf->astream = -1;
	bf->decoded_frames = 0;
	bf->video_eof = false;
	bf->frame_ready = false;
	bf->audio_stream = NULL;
	bf->audio_pcm_bytes = 0;
	bf->audio_sample_rate = 0;
	bf->audio_channels = 0;
	bf->audio_out_channels = 0;
	bf->ms_per_frame = 66;
	bf->start_ms = 0;
	bf->last_frame_ms = 0;
	bf->playback_started = false;
	bf->FrameRate = 15;
	bf->FrameRateDiv = 1;
	bf->ms_per_frame = 66;

	if (!bink_open_avformat(bf, name)) {
		delete bf;
		return NULL;
	}
	if (avformat_find_stream_info(bf->fmt, NULL) < 0) {
		bink_free(bf);
		return NULL;
	}

	for (unsigned i = 0; i < bf->fmt->nb_streams; i++) {
		if (bf->fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && bf->vstream < 0) {
			bf->vstream = (int)i;
		} else if (bf->fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && bf->astream < 0) {
			bf->astream = (int)i;
		}
	}
	if (bf->vstream < 0) {
		bink_free(bf);
		return NULL;
	}

	vpar = bf->fmt->streams[bf->vstream]->codecpar;
	vcodec = avcodec_find_decoder(vpar->codec_id);
	if (vcodec == NULL) {
		bink_free(bf);
		return NULL;
	}
	bf->vdec = avcodec_alloc_context3(vcodec);
	if (bf->vdec == NULL || avcodec_parameters_to_context(bf->vdec, vpar) < 0 ||
		avcodec_open2(bf->vdec, vcodec, NULL) < 0)
	{
		bink_free(bf);
		return NULL;
	}

	bf->Width = (unsigned long)bf->vdec->width;
	bf->Height = (unsigned long)bf->vdec->height;
	fps = bf->fmt->streams[bf->vstream]->avg_frame_rate;
	if (fps.num > 0 && fps.den > 0) {
		bf->FrameRate = (unsigned long)fps.num;
		bf->FrameRateDiv = (unsigned long)fps.den;
	}
	if (bf->FrameRate == 0) {
		bf->FrameRate = 15;
	}
	if (bf->FrameRateDiv == 0) {
		bf->FrameRateDiv = 1;
	}
	{
		const AVRational ms_time_base = {1, 1000};
		bf->ms_per_frame = (unsigned long)av_rescale_q(
			1, bf->fmt->streams[bf->vstream]->time_base, ms_time_base);
	}
	if (bf->ms_per_frame == 0) {
		bf->ms_per_frame = (unsigned long)((1000ULL * bf->FrameRateDiv + bf->FrameRate / 2ULL) / bf->FrameRate);
	}
	if (bf->ms_per_frame == 0) {
		bf->ms_per_frame = 67;
	}

	if (bf->fmt->streams[bf->vstream]->nb_frames > 0) {
		bf->Frames = (unsigned long)bf->fmt->streams[bf->vstream]->nb_frames;
	} else {
		duration_sec = (bf->fmt->duration > 0) ? (double)bf->fmt->duration / (double)AV_TIME_BASE : 0.0;
		if (duration_sec > 0.0) {
			bf->Frames = (unsigned long)(duration_sec * (double)bf->FrameRate / (double)bf->FrameRateDiv + 0.5);
		} else {
			bf->Frames = 1;
		}
	}
	if (bf->Frames == 0) {
		bf->Frames = 1;
	}

	bf->pkt = av_packet_alloc();
	bf->vframe = av_frame_alloc();
	bf->aframe = av_frame_alloc();
	if (bf->pkt == NULL || bf->vframe == NULL || bf->aframe == NULL) {
		bink_free(bf);
		return NULL;
	}

	bf->sws = NULL;

	if (bf->astream >= 0) {
		apar = bf->fmt->streams[bf->astream]->codecpar;
		acodec = avcodec_find_decoder(apar->codec_id);
		if (acodec != NULL) {
			AVChannelLayout out_layout = AV_CHANNEL_LAYOUT_STEREO;

			bf->adec = avcodec_alloc_context3(acodec);
			if (bf->adec != NULL &&
				avcodec_parameters_to_context(bf->adec, apar) == 0 &&
				avcodec_open2(bf->adec, acodec, NULL) == 0)
			{
				bf->swr = swr_alloc();
				if (bf->adec->ch_layout.nb_channels == 1) {
					out_layout = AV_CHANNEL_LAYOUT_MONO;
				}
				av_opt_set_chlayout(bf->swr, "in_chlayout", &bf->adec->ch_layout, 0);
				av_opt_set_chlayout(bf->swr, "out_chlayout", &out_layout, 0);
				av_opt_set_int(bf->swr, "in_sample_rate", bf->adec->sample_rate, 0);
				av_opt_set_int(bf->swr, "out_sample_rate", bf->adec->sample_rate, 0);
				av_opt_set_sample_fmt(bf->swr, "in_sample_fmt", bf->adec->sample_fmt, 0);
				av_opt_set_sample_fmt(bf->swr, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
				if (swr_init(bf->swr) == 0) {
					bink_init_audio_device(bf);
				}
			}
		}
	}
	bf->FrameNum = 0;
	bf->decoded_frames = 0;
	bf->video_eof = false;
	return (HBINK)bf;
}

void BinkClose(HBINK bink)
{
	bink_free(bink_from_handle(bink));
}

long BinkWait(HBINK bink)
{
	BinkFile *bf = bink_from_handle(bink);
	unsigned long now;
	unsigned long tick_ms;
	unsigned long audio_ms;
	unsigned long elapsed;
	unsigned long next_frame_ms;
	long wait;

	if (bf == NULL) {
		return 1;
	}
	if (bf->FrameNum >= bf->Frames) {
		return 1;
	}

	now = (unsigned long)SDL_GetTicks();
	tick_ms = bf->playback_started ? (now - bf->start_ms) : 0;
	audio_ms = bink_audio_elapsed_ms(bf);
	elapsed = tick_ms;
	if (bf->playback_started && audio_ms > 0) {
		elapsed = audio_ms;
	}

	next_frame_ms = bink_frame_pts_ms(bf, bf->FrameNum);
	wait = 1;
	if (elapsed + 1 >= next_frame_ms) {
		if (bf->last_frame_ms == 0 ||
			(now >= bf->last_frame_ms && now - bf->last_frame_ms >= bf->ms_per_frame))
		{
			wait = 0;
		}
	}
	return wait;
}

void BinkDoFrame(HBINK bink)
{
	BinkFile *bf = bink_from_handle(bink);
	if (bf != NULL) {
		if (bink_decode_video_frame(bf, (int)bf->FrameNum)) {
			bink_begin_playback(bf);
		}
	}
}

void BinkCopyToBuffer(HBINK bink, void *dest, long destpitch, unsigned long destheight,
	unsigned long, unsigned long, unsigned long)
{
	BinkFile *bf = bink_from_handle(bink);
	unsigned long y;
	unsigned char *dst;
	const unsigned char *src;
	unsigned long copy_w;
	unsigned long copy_h;

	(void)destheight;
	if (bf == NULL || dest == NULL || destpitch <= 0 || !bf->frame_ready || bf->rgb565.empty()) {
		return;
	}

	copy_w = bf->Width * 2u;
	copy_h = bf->Height;
	if ((unsigned long)destpitch < copy_w) {
		copy_w = (unsigned long)destpitch;
	}

	dst = (unsigned char *)dest;
	src = bf->rgb565.data();
	for (y = 0; y < copy_h; y++) {
		memcpy(dst + (size_t)y * (size_t)destpitch, src + (size_t)y * bf->Width * 2u, copy_w);
	}
}

void BinkNextFrame(HBINK bink)
{
	BinkFile *bf = bink_from_handle(bink);
	if (bf != NULL) {
		bf->last_frame_ms = (unsigned long)SDL_GetTicks();
		bf->FrameNum++;
	}
}

} /* extern "C" */
