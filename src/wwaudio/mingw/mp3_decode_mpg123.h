/*
** MP3 → 16-bit PCM via libmpg123 (memory reader + open_handle).
** Shared by audio_decode.cpp and tools/mp3_faudio_player.cpp.
*/
#ifndef MP3_DECODE_MPG123_H
#define MP3_DECODE_MPG123_H

#ifdef __cplusplus
extern "C" {
#endif

int mp3_buffer_looks_like_mp3(const void *data, unsigned long len);

#if defined(WWAUDIO_HAVE_MPG123)
int mp3_decode_to_pcm16(
	const void *data,
	unsigned long data_len,
	unsigned char **out_pcm,
	unsigned long *out_pcm_bytes,
	unsigned long *out_rate,
	int *out_channels,
	unsigned long *out_samples);

void mp3_decode_free(unsigned char *pcm);

/* Incremental decode: start playback after the first chunk (no full-file decode). */
typedef struct Mp3StreamHandle
{
	void *mh;
	unsigned char data_storage[24];
	int finished;
} Mp3StreamHandle;

int mp3_stream_begin(
	Mp3StreamHandle *stream,
	const void *data,
	unsigned long data_len,
	unsigned long *out_rate,
	int *out_channels);

/* Allocates *out_pcm; caller frees with mp3_decode_free(). Sets *done when finished. */
int mp3_stream_read_chunk(
	Mp3StreamHandle *stream,
	unsigned char **out_pcm,
	unsigned long *out_pcm_bytes,
	int *done);

void mp3_stream_end(Mp3StreamHandle *stream);
#else
static inline void mp3_decode_free(unsigned char *pcm)
{
	(void)pcm;
}
#endif

#ifdef __cplusplus
}
#endif

#endif
