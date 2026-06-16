/*
** WAV / IMA ADPCM / MP3 (FFmpeg) → PCM for SDL3 audio backend.
*/

#include "audio_decode.h"
#include "ffmpeg_decode.h"

#include "ffactory.h"
#include "wwfile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

static FileFactoryClass *g_audio_file_factory = NULL;
static audio_file_open_fn g_file_open = NULL;
static audio_file_close_fn g_file_close = NULL;
static audio_file_seek_fn g_file_seek = NULL;
static audio_file_read_fn g_file_read = NULL;

#define AIL_FILE_SEEK_BEGIN 0
#define AIL_FILE_SEEK_CURRENT 1
#define AIL_FILE_SEEK_END 2

void audio_set_file_factory(FileFactoryClass *factory)
{
	g_audio_file_factory = factory;
}

void audio_set_miles_file_callbacks(
	audio_file_open_fn open_fn,
	audio_file_close_fn close_fn,
	audio_file_seek_fn seek_fn,
	audio_file_read_fn read_fn)
{
	g_file_open = open_fn;
	g_file_close = close_fn;
	g_file_seek = seek_fn;
	g_file_read = read_fn;
}

static int audio_load_file_via_callbacks(
	const char *filename,
	unsigned char **out_data,
	unsigned long *out_len)
{
	AIL_FILE_HANDLE handle = 0;
	long size;
	unsigned long got;
	unsigned char *buf;

	if (g_file_open == NULL || g_file_read == NULL || g_file_close == NULL || g_file_seek == NULL) {
		return 0;
	}
	if (filename == NULL || out_data == NULL || out_len == NULL) {
		return 0;
	}

	*out_data = NULL;
	*out_len = 0;

	if (!g_file_open(filename, &handle) || handle == 0) {
		return 0;
	}

	g_file_seek(handle, 0, AIL_FILE_SEEK_END);
	size = g_file_seek(handle, 0, AIL_FILE_SEEK_CURRENT);
	g_file_seek(handle, 0, AIL_FILE_SEEK_BEGIN);
	if (size <= 0) {
		g_file_close(handle);
		return 0;
	}

	buf = (unsigned char *)malloc((size_t)size);
	if (buf == NULL) {
		g_file_close(handle);
		return 0;
	}

	got = g_file_read(handle, buf, (unsigned int)size);
	g_file_close(handle);
	if (got != (unsigned long)size) {
		free(buf);
		return 0;
	}

	*out_data = buf;
	*out_len = got;
	return 1;
}

static int load_from_file_object(
	FileClass *file,
	unsigned char **out_data,
	unsigned long *out_len)
{
	int size;
	int got;

	if (file == NULL || out_data == NULL || out_len == NULL) {
		return 0;
	}

	*out_data = NULL;
	*out_len = 0;

	if (!file->Is_Available()) {
		return 0;
	}
	if (!file->Is_Open()) {
		if (file->Open() != 0) {
			return 0;
		}
	}

	size = file->Size();
	if (size <= 0) {
		return 0;
	}

	*out_data = (unsigned char *)malloc((size_t)size);
	if (*out_data == NULL) {
		return 0;
	}

	got = file->Read(*out_data, size);
	if (got != size) {
		free(*out_data);
		*out_data = NULL;
		return 0;
	}

	*out_len = (unsigned long)size;
	return 1;
}

static int audio_load_file_via_factory(
	const char *filename,
	unsigned char **out_data,
	unsigned long *out_len)
{
	FileFactoryClass *factory;
	FileClass *file;
	int ok;

	if (filename == NULL) {
		return 0;
	}

	factory = g_audio_file_factory;
	if (factory == NULL) {
		factory = _TheFileFactory;
	}
	if (factory == NULL) {
		return 0;
	}

	file = factory->Get_File(filename);
	if (file == NULL) {
		return 0;
	}

	ok = load_from_file_object(file, out_data, out_len);
	if (file->Is_Open()) {
		file->Close();
	}
	factory->Return_File(file);

	return ok;
}

static int audio_load_file_via_stdio(
	const char *filename,
	unsigned char **out_data,
	unsigned long *out_len)
{
	FILE *file;
	long size;
	size_t read;

	if (filename == NULL || out_data == NULL || out_len == NULL) {
		return 0;
	}

	*out_data = NULL;
	*out_len = 0;

	file = fopen(filename, "rb");
	if (file == NULL) {
		return 0;
	}

	if (fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		return 0;
	}
	size = ftell(file);
	if (size <= 0) {
		fclose(file);
		return 0;
	}
	if (fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		return 0;
	}

	*out_data = (unsigned char *)malloc((size_t)size);
	if (*out_data == NULL) {
		fclose(file);
		return 0;
	}

	read = fread(*out_data, 1, (size_t)size, file);
	fclose(file);
	if (read != (size_t)size) {
		free(*out_data);
		*out_data = NULL;
		return 0;
	}

	*out_len = (unsigned long)size;
	return 1;
}

static unsigned short read_le16(const unsigned char *p)
{
	return (unsigned short)(p[0] | (p[1] << 8));
}

static unsigned int read_le32(const unsigned char *p)
{
	return (unsigned int)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

static const unsigned char *find_chunk(
	const unsigned char *data,
	unsigned long data_len,
	const char id[4],
	unsigned long *chunk_len)
{
	unsigned long offset = 12;

	if (data == NULL || data_len < 12 || chunk_len == NULL) {
		return NULL;
	}
	if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0) {
		return NULL;
	}

	while (offset + 8 <= data_len) {
		const unsigned char *hdr = data + offset;
		unsigned long size = read_le32(hdr + 4);

		if (memcmp(hdr, id, 4) == 0) {
			*chunk_len = size;
			return hdr + 8;
		}
		offset += 8 + ((size + 1) & ~1u);
	}
	return NULL;
}

static int adpcm_clamp_s16(int v)
{
	if (v > 32767) {
		return 32767;
	}
	if (v < -32768) {
		return -32768;
	}
	return v;
}

static int adpcm_clamp_step(int v)
{
	if (v < 0) {
		return 0;
	}
	if (v > 88) {
		return 88;
	}
	return v;
}

static int decode_ima_adpcm(
	const unsigned char *src,
	unsigned long src_len,
	unsigned short *dst,
	unsigned long dst_samples,
	int channels,
	unsigned short block_align)
{
	static const int index_table[16] = {
		-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8
	};
	static const int step_table[89] = {
		7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
		50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253,
		279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166,
		1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026,
		4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
		15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
	};

	unsigned long sample_index = 0;
	unsigned long byte_index = 0;
	int predictor[2];
	int step_index[2];

	if (channels < 1 || channels > 2 || block_align < (unsigned short)(4 * channels)) {
		return 0;
	}

	while (byte_index + block_align <= src_len && sample_index < dst_samples) {
		const unsigned char *block = src + byte_index;
		unsigned long block_offset;
		unsigned long data_bytes;

		byte_index += block_align;
		block_offset = (channels == 1) ? 4u : 8u;
		if (block_align < block_offset) {
			break;
		}
		data_bytes = (unsigned long)block_align - block_offset;

		if (channels == 1) {
			predictor[0] = (short)(block[0] | (block[1] << 8));
			step_index[0] = adpcm_clamp_step(block[2]);
			if (sample_index < dst_samples) {
				dst[sample_index++] = (unsigned short)predictor[0];
			}

			for (block_offset = 0; block_offset < data_bytes && sample_index < dst_samples; ++block_offset) {
				unsigned char nibbles = block[4 + block_offset];
				int n;

				for (n = 0; n < 2 && sample_index < dst_samples; ++n) {
					int nibble = (n == 0) ? (nibbles & 0x0f) : ((nibbles >> 4) & 0x0f);
					int step = step_table[step_index[0]];
					int diff = step >> 3;
					int predictor_val = predictor[0];

					if (nibble & 1) {
						diff += step >> 2;
					}
					if (nibble & 2) {
						diff += step >> 1;
					}
					if (nibble & 4) {
						diff += step;
					}
					if (nibble & 8) {
						diff = -diff;
					}

					predictor_val = adpcm_clamp_s16(predictor_val + diff);
					predictor[0] = predictor_val;
					step_index[0] = adpcm_clamp_step(step_index[0] + index_table[nibble & 0x0f]);
					dst[sample_index++] = (unsigned short)predictor_val;
				}
			}
		} else {
			unsigned short ch_samples[2][8];
			int ch;

			predictor[0] = (short)(block[0] | (block[1] << 8));
			step_index[0] = adpcm_clamp_step(block[2]);
			predictor[1] = (short)(block[4] | (block[5] << 8));
			step_index[1] = adpcm_clamp_step(block[6]);

			if (sample_index < dst_samples) {
				dst[sample_index++] = (unsigned short)predictor[0];
			}
			if (sample_index < dst_samples) {
				dst[sample_index++] = (unsigned short)predictor[1];
			}

			for (block_offset = 0; block_offset + 8 <= data_bytes && sample_index + 16 <= dst_samples; block_offset += 8) {
				const unsigned char *ldata = block + 8 + block_offset;
				const unsigned char *rdata = ldata + 4;
				int ch_count[2];

				ch_count[0] = 0;
				ch_count[1] = 0;

				for (ch = 0; ch < 2; ++ch) {
					const unsigned char *cdata = (ch == 0) ? ldata : rdata;
					int i;

					for (i = 0; i < 4; ++i) {
						unsigned char nibbles = cdata[i];
						int n;

						for (n = 0; n < 2; ++n) {
							int nibble = (n == 0) ? (nibbles & 0x0f) : ((nibbles >> 4) & 0x0f);
							int step = step_table[step_index[ch]];
							int diff = step >> 3;
							int predictor_val = predictor[ch];

							if (nibble & 1) {
								diff += step >> 2;
							}
							if (nibble & 2) {
								diff += step >> 1;
							}
							if (nibble & 4) {
								diff += step;
							}
							if (nibble & 8) {
								diff = -diff;
							}

							predictor_val = adpcm_clamp_s16(predictor_val + diff);
							predictor[ch] = predictor_val;
							step_index[ch] = adpcm_clamp_step(step_index[ch] + index_table[nibble & 0x0f]);
							if (ch_count[ch] < 8) {
								ch_samples[ch][ch_count[ch]++] = (unsigned short)predictor_val;
							}
						}
					}
				}

				for (ch = 0; ch < 8 && sample_index + 1 < dst_samples; ++ch) {
					dst[sample_index++] = ch_samples[0][ch];
					dst[sample_index++] = ch_samples[1][ch];
				}
			}
		}
	}

	return (int)sample_index;
}

static int upconvert_pcm8_to16(
	const unsigned char *src,
	unsigned long src_bytes,
	unsigned char **out_pcm,
	unsigned long *out_bytes)
{
	unsigned long samples = src_bytes;
	unsigned short *dst;
	unsigned long i;

	if (src == NULL || out_pcm == NULL || out_bytes == NULL || src_bytes == 0) {
		return 0;
	}

	dst = (unsigned short *)malloc(samples * sizeof(unsigned short));
	if (dst == NULL) {
		return 0;
	}

	/*
	** WAV 8-bit PCM is unsigned (128 = silence). Signed-char conversion was scratch/noise
	** on reload SFX (logs: pistol_reload.wav 37810 -> 75532 PCM).
	*/
	for (i = 0; i < samples; ++i) {
		int sample = (int)src[i] - 128;
		dst[i] = (unsigned short)(sample << 8);
	}

	*out_pcm = (unsigned char *)dst;
	*out_bytes = samples * sizeof(unsigned short);
	return 1;
}

static int probe_wav(const void *data, unsigned long data_len, AILSOUNDINFO *info)
{
	unsigned long fmt_len = 0;
	const unsigned char *fmt = find_chunk((const unsigned char *)data, data_len, "fmt ", &fmt_len);
	unsigned long data_chunk_len = 0;
	const unsigned char *pcm = find_chunk((const unsigned char *)data, data_len, "data", &data_chunk_len);

	if (fmt == NULL || fmt_len < 16) {
		return 0;
	}

	memset(info, 0, sizeof(*info));
	info->format = (S32)read_le16(fmt + 0);
	info->channels = (S32)read_le16(fmt + 2);
	info->rate = read_le32(fmt + 4);
	info->bits = (S32)read_le16(fmt + 14);
	info->data_ptr = pcm;
	info->data_len = data_chunk_len;
	info->initial_ptr = data;

	if (info->format == WAVE_FORMAT_IMA_ADPCM && fmt_len >= 20) {
		info->block_size = read_le16(fmt + 12);
	}

	if (info->bits > 0 && info->channels > 0 && info->rate > 0) {
		if (info->format == WAVE_FORMAT_PCM) {
			info->samples = data_chunk_len / (unsigned int)((info->bits >> 3) * info->channels);
		} else if (info->format == WAVE_FORMAT_IMA_ADPCM && info->block_size > 0) {
			unsigned int samples_per_block = ((info->block_size - (4 * info->channels)) * 2 / info->channels) + 1;
			unsigned int blocks = data_chunk_len / info->block_size;
			info->samples = blocks * samples_per_block;
		}
	}
	return 1;
}

int audio_decode_probe(const void *data, unsigned long data_len, AILSOUNDINFO *info)
{
	if (data == NULL || info == NULL || data_len == 0) {
		return 0;
	}

	if (probe_wav(data, data_len, info)) {
		return 1;
	}

	if (ffmpeg_buffer_looks_like_mp3(data, data_len)) {
		memset(info, 0, sizeof(*info));
		info->format = 0x0055;
		info->rate = 44100;
		info->bits = 16;
		info->channels = 2;
		info->data_ptr = data;
		info->data_len = data_len;
		return 1;
	}

	return 0;
}

#if defined(WWAUDIO_HAVE_FFMPEG)
static int decode_mp3_ffmpeg(
	const void *data,
	unsigned long data_len,
	unsigned char **out_pcm,
	unsigned long *out_pcm_bytes,
	AILSOUNDINFO *info)
{
	unsigned long rate = 0;
	int channels = 0;
	unsigned long samples = 0;

	if (!ffmpeg_decode_buffer_to_pcm16(
			data,
			data_len,
			out_pcm,
			out_pcm_bytes,
			&rate,
			&channels,
			&samples))
	{
		return 0;
	}

	memset(info, 0, sizeof(*info));
	info->format = WAVE_FORMAT_PCM;
	info->rate = (U32)rate;
	info->bits = 16;
	info->channels = channels;
	info->data_ptr = *out_pcm;
	info->data_len = (U32)*out_pcm_bytes;
	info->samples = (U32)samples;
	return 1;
}
#endif

int audio_decode_to_pcm(
	const void *data,
	unsigned long data_len,
	unsigned char **out_pcm,
	unsigned long *out_pcm_bytes,
	AILSOUNDINFO *info)
{
	AILSOUNDINFO probe;

	if (out_pcm == NULL || out_pcm_bytes == NULL || info == NULL || data == NULL || data_len == 0) {
		return 0;
	}

	*out_pcm = NULL;
	*out_pcm_bytes = 0;

	if (ffmpeg_buffer_looks_like_mp3(data, data_len)) {
#if defined(WWAUDIO_HAVE_FFMPEG)
		return decode_mp3_ffmpeg(data, data_len, out_pcm, out_pcm_bytes, info);
#else
		return 0;
#endif
	}

	if (!audio_decode_probe(data, data_len, &probe)) {
#if defined(WWAUDIO_HAVE_FFMPEG)
		/* FFmpeg handles other containers (e.g. audio in odd blobs). */
		return decode_mp3_ffmpeg(data, data_len, out_pcm, out_pcm_bytes, info);
#else
		return 0;
#endif
	}

	if (probe.format == WAVE_FORMAT_PCM) {
		unsigned long bytes = probe.data_len;
		unsigned char *pcm = NULL;

		if (probe.bits == 8) {
			if (!upconvert_pcm8_to16(
					(const unsigned char *)probe.data_ptr,
					bytes,
					&pcm,
					out_pcm_bytes))
			{
				return 0;
			}
			*out_pcm = pcm;
			*info = probe;
			info->format = WAVE_FORMAT_PCM;
			info->bits = 16;
			info->data_ptr = pcm;
			info->data_len = *out_pcm_bytes;
			return 1;
		}

		pcm = (unsigned char *)malloc(bytes);
		if (pcm == NULL) {
			return 0;
		}
		memcpy(pcm, probe.data_ptr, bytes);
		*out_pcm = pcm;
		*out_pcm_bytes = bytes;
		*info = probe;
		info->format = WAVE_FORMAT_PCM;
		info->data_ptr = pcm;
		info->data_len = bytes;
		return 1;
	}

	if (probe.format == WAVE_FORMAT_IMA_ADPCM) {
		unsigned long ch = (probe.channels > 0) ? (unsigned long)probe.channels : 1;
		unsigned long max_samples = probe.samples * ch;
		unsigned long pcm_bytes = max_samples * sizeof(unsigned short);
		unsigned long decoded;
		unsigned short *dst = (unsigned short *)malloc(pcm_bytes);

		if (dst == NULL || max_samples == 0) {
			free(dst);
			return 0;
		}
		memset(dst, 0, pcm_bytes);

		decoded = decode_ima_adpcm(
			(const unsigned char *)probe.data_ptr,
			probe.data_len,
			dst,
			max_samples,
			probe.channels,
			(unsigned short)probe.block_size);
		if (decoded == 0) {
			free(dst);
			return 0;
		}

		*out_pcm = (unsigned char *)dst;
		*out_pcm_bytes = decoded * sizeof(unsigned short);
		*info = probe;
		info->format = WAVE_FORMAT_PCM;
		info->bits = 16;
		info->data_ptr = *out_pcm;
		info->data_len = *out_pcm_bytes;
		if (ch > 0) {
			info->samples = decoded / ch;
		}
		return 1;
	}

	return 0;
}

void audio_decode_free(unsigned char *pcm)
{
	free(pcm);
}

int audio_pcm_resample_to_rate(
	unsigned char **pcm,
	unsigned long *pcm_bytes,
	int *rate_hz,
	int channels,
	int target_rate_hz)
{
	SwrContext *swr;
	AVChannelLayout in_layout;
	AVChannelLayout out_layout;
	int in_samples;
	int out_cap;
	int out_samples;
	unsigned char *out_buf;
	const uint8_t *in_planes[1];
	uint8_t *out_planes[1];
	int converted;

	if (pcm == NULL || pcm_bytes == NULL || rate_hz == NULL || *pcm == NULL) {
		return 0;
	}
	if (target_rate_hz <= 0 || channels <= 0) {
		return 0;
	}
	if (*rate_hz == target_rate_hz) {
		return 1;
	}

	if (channels >= 2) {
		av_channel_layout_default(&in_layout, 2);
		av_channel_layout_default(&out_layout, 2);
		channels = 2;
	} else {
		av_channel_layout_default(&in_layout, 1);
		av_channel_layout_default(&out_layout, 1);
		channels = 1;
	}

	in_samples = (int)(*pcm_bytes / (unsigned long)(channels * 2));
	if (in_samples <= 0) {
		return 0;
	}

	swr = swr_alloc();
	if (swr == NULL) {
		return 0;
	}
	av_opt_set_chlayout(swr, "in_chlayout", &in_layout, 0);
	av_opt_set_chlayout(swr, "out_chlayout", &out_layout, 0);
	av_opt_set_int(swr, "in_sample_rate", *rate_hz, 0);
	av_opt_set_int(swr, "out_sample_rate", target_rate_hz, 0);
	av_opt_set_sample_fmt(swr, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
	av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
	if (swr_init(swr) < 0) {
		swr_free(&swr);
		return 0;
	}

	out_cap = (int)av_rescale_rnd(in_samples, target_rate_hz, *rate_hz, AV_ROUND_UP) + 8;
	out_buf = (unsigned char *)malloc((size_t)out_cap * (size_t)channels * 2u);
	if (out_buf == NULL) {
		swr_free(&swr);
		return 0;
	}

	in_planes[0] = *pcm;
	out_planes[0] = out_buf;
	converted = swr_convert(swr, out_planes, out_cap, in_planes, in_samples);
	swr_free(&swr);

	if (converted <= 0) {
		free(out_buf);
		return 0;
	}

	out_samples = converted;
	audio_decode_free(*pcm);
	*pcm = out_buf;
	*pcm_bytes = (unsigned long)out_samples * (unsigned long)channels * 2u;
	*rate_hz = target_rate_hz;
	return 1;
}

int audio_load_file(const char *filename, unsigned char **out_data, unsigned long *out_len)
{
	if (filename == NULL || out_data == NULL || out_len == NULL) {
		return 0;
	}

	*out_data = NULL;
	*out_len = 0;

	if (audio_load_file_via_callbacks(filename, out_data, out_len)) {
		return 1;
	}

	if (audio_load_file_via_factory(filename, out_data, out_len)) {
		return 1;
	}

	return audio_load_file_via_stdio(filename, out_data, out_len);
}
