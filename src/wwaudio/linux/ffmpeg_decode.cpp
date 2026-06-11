/*
 * FFmpeg decode: MP3, WAV containers, Bink (.bik) audio track → 16-bit PCM.
 */

#include "ffmpeg_decode.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include <cstdlib>
#include <cstring>

struct FfmpegMemIO {
	const unsigned char *data;
	unsigned long size;
	unsigned long pos;
};

static int ffmpeg_mem_read(void *opaque, uint8_t *buf, int buf_size)
{
	FfmpegMemIO *io = (FfmpegMemIO *)opaque;
	if (io == NULL || buf == NULL || buf_size <= 0) {
		return AVERROR_EOF;
	}
	if (io->pos >= io->size) {
		return AVERROR_EOF;
	}
	const unsigned long remain = io->size - io->pos;
	const int to_read = (int)((remain < (unsigned long)buf_size) ? remain : (unsigned long)buf_size);
	std::memcpy(buf, io->data + io->pos, (size_t)to_read);
	io->pos += (unsigned long)to_read;
	return to_read;
}

static int64_t ffmpeg_mem_seek(void *opaque, int64_t offset, int whence)
{
	FfmpegMemIO *io = (FfmpegMemIO *)opaque;
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

int ffmpeg_buffer_looks_like_mp3(const void *data, unsigned long data_len)
{
	const unsigned char *d = (const unsigned char *)data;
	if (data == NULL || data_len < 3) {
		return 0;
	}
	if (d[0] == 'I' && d[1] == 'D' && d[2] == '3') {
		return 1;
	}
	if (d[0] == 0xFF && (d[1] & 0xE0) == 0xE0) {
		return 1;
	}
	return 0;
}

static int ffmpeg_decode_avformat(
	AVFormatContext *fmt,
	unsigned char **out_pcm,
	unsigned long *out_pcm_bytes,
	unsigned long *out_rate,
	int *out_channels,
	unsigned long *out_samples)
{
	AVCodecContext *dec = NULL;
	const AVCodec *codec = NULL;
	SwrContext *swr = NULL;
	AVPacket *pkt = NULL;
	AVFrame *frame = NULL;
	int audio_stream = -1;
	unsigned char *pcm = NULL;
	unsigned long pcm_cap = 0;
	unsigned long pcm_len = 0;
	int ret = 0;

	if (out_pcm == NULL || out_pcm_bytes == NULL) {
		return 0;
	}
	*out_pcm = NULL;
	*out_pcm_bytes = 0;
	if (out_rate) {
		*out_rate = 0;
	}
	if (out_channels) {
		*out_channels = 0;
	}
	if (out_samples) {
		*out_samples = 0;
	}

	if (avformat_find_stream_info(fmt, NULL) < 0) {
		return 0;
	}

	for (unsigned i = 0; i < fmt->nb_streams; i++) {
		if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			audio_stream = (int)i;
			break;
		}
	}
	if (audio_stream < 0) {
		return 0;
	}

	{
		const AVCodecParameters *par = fmt->streams[audio_stream]->codecpar;
		codec = avcodec_find_decoder(par->codec_id);
		if (codec == NULL) {
			return 0;
		}
		dec = avcodec_alloc_context3(codec);
		if (dec == NULL) {
			return 0;
		}
		if (avcodec_parameters_to_context(dec, par) < 0) {
			avcodec_free_context(&dec);
			return 0;
		}
		if (avcodec_open2(dec, codec, NULL) < 0) {
			avcodec_free_context(&dec);
			return 0;
		}
	}

	{
		AVChannelLayout out_layout = AV_CHANNEL_LAYOUT_STEREO;
		if (dec->ch_layout.nb_channels == 1) {
			out_layout = AV_CHANNEL_LAYOUT_MONO;
		}
		swr = swr_alloc();
		if (swr == NULL) {
			avcodec_free_context(&dec);
			return 0;
		}
		av_opt_set_chlayout(swr, "in_chlayout", &dec->ch_layout, 0);
		av_opt_set_chlayout(swr, "out_chlayout", &out_layout, 0);
		av_opt_set_int(swr, "in_sample_rate", dec->sample_rate, 0);
		av_opt_set_int(swr, "out_sample_rate", dec->sample_rate, 0);
		av_opt_set_sample_fmt(swr, "in_sample_fmt", dec->sample_fmt, 0);
		av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
		if (swr_init(swr) < 0) {
			swr_free(&swr);
			avcodec_free_context(&dec);
			return 0;
		}
	}

	pkt = av_packet_alloc();
	frame = av_frame_alloc();
	if (pkt == NULL || frame == NULL) {
		ret = 0;
		goto done;
	}

	while (av_read_frame(fmt, pkt) >= 0) {
		if (pkt->stream_index != audio_stream) {
			av_packet_unref(pkt);
			continue;
		}
		if (avcodec_send_packet(dec, pkt) < 0) {
			av_packet_unref(pkt);
			continue;
		}
		av_packet_unref(pkt);

		while (avcodec_receive_frame(dec, frame) == 0) {
			const int out_samples_count = swr_get_out_samples(swr, frame->nb_samples);
			const unsigned long need = pcm_len + (unsigned long)out_samples_count * 2u * 2u;
			unsigned char *grown;

			if (need > pcm_cap) {
				pcm_cap = need + 65536u;
				grown = (unsigned char *)std::realloc(pcm, pcm_cap);
				if (grown == NULL) {
					ret = 0;
					goto done;
				}
				pcm = grown;
			}

			{
				uint8_t *out_planes[1] = { pcm + pcm_len };
				const int converted = swr_convert(
					swr,
					out_planes,
					out_samples_count,
					(const uint8_t **)frame->data,
					frame->nb_samples);
				if (converted > 0) {
					const int out_ch = (dec->ch_layout.nb_channels == 1) ? 1 : 2;
					pcm_len += (unsigned long)converted * (unsigned long)out_ch * 2u;
				}
			}
			av_frame_unref(frame);
		}
	}

	/* Flush decoder. */
	avcodec_send_packet(dec, NULL);
	while (avcodec_receive_frame(dec, frame) == 0) {
		const int out_samples_count = swr_get_out_samples(swr, frame->nb_samples);
		const unsigned long need = pcm_len + (unsigned long)out_samples_count * 2u * 2u;
		unsigned char *grown;

		if (need > pcm_cap) {
			pcm_cap = need + 65536u;
			grown = (unsigned char *)std::realloc(pcm, pcm_cap);
			if (grown == NULL) {
				ret = 0;
				goto done;
			}
			pcm = grown;
		}
		{
			uint8_t *out_planes[1] = { pcm + pcm_len };
			const int converted = swr_convert(
				swr,
				out_planes,
				out_samples_count,
				(const uint8_t **)frame->data,
				frame->nb_samples);
			if (converted > 0) {
				const int out_ch = (dec->ch_layout.nb_channels == 1) ? 1 : 2;
				pcm_len += (unsigned long)converted * (unsigned long)out_ch * 2u;
			}
		}
		av_frame_unref(frame);
	}

	if (pcm_len == 0) {
		ret = 0;
		goto done;
	}

	*out_pcm = pcm;
	pcm = NULL;
	*out_pcm_bytes = pcm_len;
	if (out_rate) {
		*out_rate = (unsigned long)dec->sample_rate;
	}
	if (out_channels) {
		*out_channels = (dec->ch_layout.nb_channels == 1) ? 1 : 2;
	}
	if (out_samples && dec->ch_layout.nb_channels > 0) {
		const int ch = (dec->ch_layout.nb_channels == 1) ? 1 : 2;
		*out_samples = pcm_len / ((unsigned long)ch * 2u);
	}
	ret = 1;

done:
	if (pcm != NULL) {
		std::free(pcm);
	}
	if (frame != NULL) {
		av_frame_free(&frame);
	}
	if (pkt != NULL) {
		av_packet_free(&pkt);
	}
	if (swr != NULL) {
		swr_free(&swr);
	}
	if (dec != NULL) {
		avcodec_free_context(&dec);
	}
	return ret;
}

int ffmpeg_decode_buffer_to_pcm16(
	const void *data,
	unsigned long data_len,
	unsigned char **out_pcm,
	unsigned long *out_pcm_bytes,
	unsigned long *out_rate,
	int *out_channels,
	unsigned long *out_samples)
{
	FfmpegMemIO mem;
	AVFormatContext *fmt = NULL;
	AVIOContext *avio = NULL;
	unsigned char *avio_buf = NULL;
	int ret = 0;

	if (data == NULL || data_len == 0) {
		return 0;
	}

	mem.data = (const unsigned char *)data;
	mem.size = data_len;
	mem.pos = 0;

	avio_buf = (unsigned char *)av_malloc(4096);
	if (avio_buf == NULL) {
		return 0;
	}
	avio = avio_alloc_context(
		avio_buf,
		4096,
		0,
		&mem,
		ffmpeg_mem_read,
		NULL,
		ffmpeg_mem_seek);
	if (avio == NULL) {
		av_free(avio_buf);
		return 0;
	}

	fmt = avformat_alloc_context();
	if (fmt == NULL) {
		avio_context_free(&avio);
		return 0;
	}
	fmt->pb = avio;

	if (avformat_open_input(&fmt, NULL, NULL, NULL) < 0) {
		avformat_close_input(&fmt);
		return 0;
	}

	ret = ffmpeg_decode_avformat(fmt, out_pcm, out_pcm_bytes, out_rate, out_channels, out_samples);
	avformat_close_input(&fmt);
	return ret;
}

int ffmpeg_decode_file_to_pcm16(
	const char *path,
	unsigned char **out_pcm,
	unsigned long *out_pcm_bytes,
	unsigned long *out_rate,
	int *out_channels,
	unsigned long *out_samples)
{
	AVFormatContext *fmt = NULL;
	int ret;

	if (path == NULL || path[0] == '\0') {
		return 0;
	}
	if (avformat_open_input(&fmt, path, NULL, NULL) < 0) {
		return 0;
	}
	ret = ffmpeg_decode_avformat(fmt, out_pcm, out_pcm_bytes, out_rate, out_channels, out_samples);
	avformat_close_input(&fmt);
	return ret;
}
