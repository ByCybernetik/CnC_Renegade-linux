#ifndef FFMPEG_DECODE_H
#define FFMPEG_DECODE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Decode an in-memory audio blob (MP3, WAV in container, etc.) to 16-bit PCM. */
int ffmpeg_decode_buffer_to_pcm16(
	const void *data,
	unsigned long data_len,
	unsigned char **out_pcm,
	unsigned long *out_pcm_bytes,
	unsigned long *out_rate,
	int *out_channels,
	unsigned long *out_samples);

/* Decode audio track from a file path (.mp3, .wav, .bik, …). */
int ffmpeg_decode_file_to_pcm16(
	const char *path,
	unsigned char **out_pcm,
	unsigned long *out_pcm_bytes,
	unsigned long *out_rate,
	int *out_channels,
	unsigned long *out_samples);

int ffmpeg_buffer_looks_like_mp3(const void *data, unsigned long data_len);

#ifdef __cplusplus
}
#endif

#endif
