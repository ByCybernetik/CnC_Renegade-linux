/*
** Decode game audio blobs (WAV PCM, IMA ADPCM, MP3) to PCM for OpenAL.
*/
#ifndef AUDIO_DECODE_H
#define AUDIO_DECODE_H

#include "mss_stub.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Inspect container; fills AILSOUNDINFO when recognized. Returns 0 on failure. */
int audio_decode_probe(const void *data, unsigned long data_len, AILSOUNDINFO *info);

/*
** Decode full blob to 16-bit PCM (interleaved if stereo).
** Caller frees *out_pcm with audio_decode_free().
*/
int audio_decode_to_pcm(
	const void *data,
	unsigned long data_len,
	unsigned char **out_pcm,
	unsigned long *out_pcm_bytes,
	AILSOUNDINFO *info);

void audio_decode_free(unsigned char *pcm);

/*
** Resample 16-bit PCM in-place to target_rate_hz (stereo interleaved).
** Updates *pcm, *pcm_bytes, *rate_hz on success.
*/
int audio_pcm_resample_to_rate(
	unsigned char **pcm,
	unsigned long *pcm_bytes,
	int *rate_hz,
	int channels,
	int target_rate_hz);

/* Miles file callbacks from WWAudio::Initialize (MIX / archive paths). */
typedef unsigned int AIL_FILE_HANDLE;
typedef int (*audio_file_open_fn)(const char *filename, AIL_FILE_HANDLE *file_handle);
typedef void (*audio_file_close_fn)(AIL_FILE_HANDLE file_handle);
typedef int (*audio_file_seek_fn)(AIL_FILE_HANDLE file_handle, int offset, unsigned int type);
typedef unsigned int (*audio_file_read_fn)(AIL_FILE_HANDLE file_handle, void *buffer, unsigned int bytes);

void audio_set_miles_file_callbacks(
	audio_file_open_fn open_fn,
	audio_file_close_fn close_fn,
	audio_file_seek_fn seek_fn,
	audio_file_read_fn read_fn);

/* Game file factory fallback when callbacks are unavailable. */
class FileFactoryClass;
void audio_set_file_factory(FileFactoryClass *factory);

/* Read a file from disk (streaming Miles paths). Caller frees with audio_decode_free(). */
int audio_load_file(const char *filename, unsigned char **out_data, unsigned long *out_len);

#ifdef __cplusplus
}
#endif

#endif
