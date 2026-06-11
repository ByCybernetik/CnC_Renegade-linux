/*
** MP3 → 16-bit PCM (mpg123_replace_reader_handle + mpg123_open_handle).
*/

#include "mp3_decode_mpg123.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(WWAUDIO_HAVE_MPG123)
#include <mpg123.h>
#endif

int mp3_buffer_looks_like_mp3(const void *data, unsigned long len)
{
	const unsigned char *p = (const unsigned char *)data;

	if (p == NULL || len < 4) {
		return 0;
	}
	if (p[0] == 'I' && p[1] == 'D' && p[2] == '3') {
		return 1;
	}
	if (p[0] == 0xff && (p[1] & 0xe0) == 0xe0) {
		return 1;
	}
	return 0;
}

#if defined(WWAUDIO_HAVE_MPG123)

static int g_mpg123_initialized = 0;

static int mp3_ensure_mpg123_init(void)
{
	if (g_mpg123_initialized) {
		return 1;
	}
	if (mpg123_init() != MPG123_OK) {
		return 0;
	}
	g_mpg123_initialized = 1;
	return 1;
}

typedef struct Mp3MemIO {
	const unsigned char *data;
	size_t size;
	size_t pos;
} Mp3MemIO;

static mpg123_ssize_t mp3_mem_read(void *handle, void *buf, size_t count)
{
	Mp3MemIO *io = (Mp3MemIO *)handle;
	size_t avail;

	if (io == NULL || buf == NULL) {
		return -1;
	}
	avail = io->size - io->pos;
	if (avail == 0) {
		return 0;
	}
	if (count > avail) {
		count = avail;
	}
	memcpy(buf, io->data + io->pos, count);
	io->pos += count;
	return (mpg123_ssize_t)count;
}

static off_t mp3_mem_lseek(void *handle, off_t offset, int whence)
{
	Mp3MemIO *io = (Mp3MemIO *)handle;
	off_t pos;

	if (io == NULL) {
		return -1;
	}
	switch (whence) {
	case SEEK_SET:
		pos = offset;
		break;
	case SEEK_CUR:
		pos = (off_t)io->pos + offset;
		break;
	case SEEK_END:
		pos = (off_t)io->size + offset;
		break;
	default:
		return -1;
	}
	if (pos < 0 || (size_t)pos > io->size) {
		return -1;
	}
	io->pos = (size_t)pos;
	return pos;
}

static int mp3_open_mem_decoder(
	Mp3MemIO *io,
	const void *data,
	unsigned long data_len,
	mpg123_handle **out_mh,
	long *out_rate,
	int *out_channels)
{
	mpg123_handle *mh = NULL;
	int err = MPG123_OK;
	int encoding = 0;
	long rate = 0;
	int channels = 0;

	if (io == NULL || out_mh == NULL || data == NULL || data_len == 0) {
		return 0;
	}

	io->data = (const unsigned char *)data;
	io->size = (size_t)data_len;
	io->pos = 0;

	if (!mp3_ensure_mpg123_init()) {
		return 0;
	}

	mh = mpg123_new(NULL, &err);
	if (mh == NULL) {
		return 0;
	}

	if (mpg123_replace_reader_handle(mh, mp3_mem_read, mp3_mem_lseek, NULL) != MPG123_OK) {
		mpg123_delete(mh);
		return 0;
	}

	if (mpg123_open_handle(mh, io) != MPG123_OK) {
		mpg123_delete(mh);
		return 0;
	}

	mpg123_format_none(mh);
	if (mpg123_format2(mh, 0, MPG123_STEREO | MPG123_MONO, MPG123_ENC_SIGNED_16) != MPG123_OK) {
		mpg123_close(mh);
		mpg123_delete(mh);
		return 0;
	}

	if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
		rate = 44100;
		channels = 2;
	}

	*out_mh = mh;
	if (out_rate) {
		*out_rate = (unsigned long)rate;
	}
	if (out_channels) {
		*out_channels = channels;
	}
	return 1;
}

int mp3_stream_begin(
	Mp3StreamHandle *stream,
	const void *data,
	unsigned long data_len,
	unsigned long *out_rate,
	int *out_channels)
{
	Mp3MemIO *io;
	mpg123_handle *mh = NULL;

	if (stream == NULL) {
		return 0;
	}

	memset(stream, 0, sizeof(*stream));
	io = (Mp3MemIO *)stream->data_storage;

	{
		long rate = 0;
		int channels = 0;

		if (!mp3_open_mem_decoder(io, data, data_len, &mh, &rate, &channels)) {
			return 0;
		}
		if (out_rate) {
			*out_rate = (unsigned long)rate;
		}
		if (out_channels) {
			*out_channels = channels;
		}
	}

	stream->mh = mh;
	return 1;
}

int mp3_stream_read_chunk(
	Mp3StreamHandle *stream,
	unsigned char **out_pcm,
	unsigned long *out_pcm_bytes,
	int *done)
{
	mpg123_handle *mh;
	unsigned char *pcm = NULL;
	size_t pcm_size = 0;

	if (stream == NULL || out_pcm == NULL || out_pcm_bytes == NULL || done == NULL) {
		return 0;
	}

	*out_pcm = NULL;
	*out_pcm_bytes = 0;
	*done = stream->finished;

	if (stream->finished || stream->mh == NULL) {
		*done = 1;
		return 0;
	}

	mh = (mpg123_handle *)stream->mh;

	for (;;) {
		unsigned char buffer[65536];
		size_t got = 0;
		int ret = mpg123_read(mh, buffer, sizeof(buffer), &got);

		if (got > 0) {
			unsigned char *grown = (unsigned char *)realloc(pcm, pcm_size + got);
			if (grown == NULL) {
				free(pcm);
				return 0;
			}
			pcm = grown;
			memcpy(pcm + pcm_size, buffer, got);
			pcm_size += got;
		}

		if (ret == MPG123_DONE) {
			stream->finished = 1;
			*done = 1;
			break;
		}
		if (ret == MPG123_NEW_FORMAT) {
			continue;
		}
		if (ret != MPG123_OK) {
			stream->finished = 1;
			*done = 1;
			break;
		}
		if (pcm_size > 0) {
			break;
		}
	}

	if (pcm == NULL || pcm_size == 0) {
		free(pcm);
		return 0;
	}

	*out_pcm = pcm;
	*out_pcm_bytes = (unsigned long)pcm_size;
	return 1;
}

void mp3_stream_end(Mp3StreamHandle *stream)
{
	mpg123_handle *mh;

	if (stream == NULL) {
		return;
	}

	mh = (mpg123_handle *)stream->mh;
	if (mh != NULL) {
		mpg123_close(mh);
		mpg123_delete(mh);
		stream->mh = NULL;
	}
	stream->finished = 1;
}

int mp3_decode_to_pcm16(
	const void *data,
	unsigned long data_len,
	unsigned char **out_pcm,
	unsigned long *out_pcm_bytes,
	unsigned long *out_rate,
	int *out_channels,
	unsigned long *out_samples)
{
	Mp3MemIO io;
	mpg123_handle *mh = NULL;
	long rate = 0;
	int channels = 0;
	int encoding = 0;
	unsigned char *pcm = NULL;
	size_t pcm_size = 0;

	if (out_pcm == NULL || out_pcm_bytes == NULL || data == NULL || data_len == 0) {
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

	if (!mp3_open_mem_decoder(&io, data, data_len, &mh, &rate, &channels)) {
		return 0;
	}

	for (;;) {
		unsigned char buffer[65536];
		size_t got = 0;
		int ret = mpg123_read(mh, buffer, sizeof(buffer), &got);

		if (got > 0) {
			unsigned char *grown = (unsigned char *)realloc(pcm, pcm_size + got);
			if (grown == NULL) {
				free(pcm);
				mpg123_close(mh);
				mpg123_delete(mh);
				return 0;
			}
			pcm = grown;
			memcpy(pcm + pcm_size, buffer, got);
			pcm_size += got;
		}

		if (ret == MPG123_DONE) {
			break;
		}
		if (ret == MPG123_NEW_FORMAT) {
			if (mpg123_getformat(mh, &rate, &channels, &encoding) == MPG123_OK) {
				continue;
			}
			break;
		}
		if (ret != MPG123_OK) {
			break;
		}
	}

	if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
		rate = 44100;
		channels = 2;
	}

	mpg123_close(mh);
	mpg123_delete(mh);

	if (pcm == NULL || pcm_size == 0 || channels < 1) {
		free(pcm);
		return 0;
	}

	*out_pcm = pcm;
	*out_pcm_bytes = (unsigned long)pcm_size;
	if (out_rate) {
		*out_rate = (unsigned long)rate;
	}
	if (out_channels) {
		*out_channels = channels;
	}
	if (out_samples) {
		*out_samples = (unsigned long)(pcm_size / (size_t)(2 * channels));
	}
	return 1;
}

void mp3_decode_free(unsigned char *pcm)
{
	free(pcm);
}

#endif
