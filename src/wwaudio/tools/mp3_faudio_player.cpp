/*
** Minimal MP3 test player: mpg123 (stream or full decode) + FAudio.
** Same decode path as Renegade menu music; isolates game audio stack issues.
**
** Usage (Wine):
**   mp3_faudio_player.exe menu.mp3
**   mp3_faudio_player.exe menu.mp3 --loop
**   mp3_faudio_player.exe menu.mp3 --seconds 15
**   mp3_faudio_player.exe menu.mp3 --preload   (full decode before play, old behaviour)
**
** Copy libFAudio_shared-0.dll next to the .exe (see build-mingw/subprojects/faudio/).
*/

#include "mp3_decode_mpg123.h"

#include <windows.h>
#include <FAudio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FAUDIO_COMMIT_NOW
#define FAUDIO_COMMIT_NOW 0
#endif

#ifndef FAUDIO_END_OF_STREAM
#define FAUDIO_END_OF_STREAM 0x0040
#endif

struct LiveBuffer
{
	unsigned char *pcm;
	unsigned long pcm_bytes;
};

static void fill_wfx(FAudioWaveFormatEx *wfx, int rate, int channels, int bits)
{
	memset(wfx, 0, sizeof(*wfx));
	wfx->wFormatTag = 1;
	wfx->nChannels = (WORD)((channels > 0) ? channels : 1);
	wfx->nSamplesPerSec = (DWORD)((rate > 0) ? rate : 44100);
	wfx->wBitsPerSample = (WORD)((bits > 0) ? bits : 16);
	wfx->nBlockAlign = (WORD)(wfx->nChannels * (wfx->wBitsPerSample / 8));
	wfx->nAvgBytesPerSec = wfx->nSamplesPerSec * wfx->nBlockAlign;
	wfx->cbSize = 0;
}

static int load_file(const char *path, unsigned char **out_data, unsigned long *out_len)
{
	HANDLE file;
	DWORD size;
	DWORD read;
	unsigned char *buf;

	if (path == NULL || out_data == NULL || out_len == NULL) {
		return 0;
	}

	*out_data = NULL;
	*out_len = 0;

	file = CreateFileA(
		path,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (file == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "open failed: %s (error %lu)\n", path, (unsigned long)GetLastError());
		return 0;
	}

	size = GetFileSize(file, NULL);
	if (size == 0 || size == INVALID_FILE_SIZE) {
		CloseHandle(file);
		fprintf(stderr, "invalid file size: %s\n", path);
		return 0;
	}

	buf = (unsigned char *)malloc(size);
	if (buf == NULL) {
		CloseHandle(file);
		return 0;
	}

	if (!ReadFile(file, buf, size, &read, NULL) || read != size) {
		free(buf);
		CloseHandle(file);
		fprintf(stderr, "read failed: %s\n", path);
		return 0;
	}

	CloseHandle(file);
	*out_data = buf;
	*out_len = (unsigned long)size;
	return 1;
}

static int upmix_mono_chunk(
	const unsigned char *mono,
	unsigned long mono_bytes,
	unsigned char **out_pcm,
	unsigned long *out_pcm_bytes)
{
	unsigned long n;
	unsigned char *out;
	const unsigned short *src;
	unsigned short *dst;
	unsigned long i;

	if (mono == NULL || mono_bytes < 2 || out_pcm == NULL || out_pcm_bytes == NULL) {
		return 0;
	}

	n = mono_bytes / 2;
	out = (unsigned char *)malloc(mono_bytes * 2);
	if (out == NULL) {
		return 0;
	}

	src = (const unsigned short *)mono;
	dst = (unsigned short *)out;
	for (i = 0; i < n; i++) {
		dst[i * 2] = src[i];
		dst[i * 2 + 1] = src[i];
	}

	*out_pcm = out;
	*out_pcm_bytes = mono_bytes * 2;
	return 1;
}

static void apply_stereo_output_matrix(FAudioSourceVoice *source)
{
	float matrix[4];

	if (source == NULL) {
		return;
	}

	matrix[0] = 1.0f;
	matrix[1] = 0.0f;
	matrix[2] = 0.0f;
	matrix[3] = 1.0f;
	FAudioVoice_SetOutputMatrix(source, NULL, 2, 2, matrix, FAUDIO_COMMIT_NOW);
}

static void wait_until_done(FAudioSourceVoice *source, UINT64 total_samples, int loop, DWORD max_ms)
{
	FAudioVoiceState state;
	DWORD waited = 0;

	for (;;) {
		memset(&state, 0, sizeof(state));
		FAudioSourceVoice_GetState(source, &state, 0);

		if (!loop && state.BuffersQueued == 0) {
			if (total_samples == 0 || state.SamplesPlayed >= total_samples) {
				break;
			}
		}

		if (max_ms > 0) {
			waited += 50;
			if (waited >= max_ms) {
				break;
			}
		} else if (loop) {
			/* --loop without --seconds: run until Ctrl+C; poll forever. */
		} else if (total_samples > 0 && state.SamplesPlayed >= total_samples) {
			break;
		}

		Sleep(50);
	}
}

static int play_streamed(
	const unsigned char *file_data,
	unsigned long file_len,
	int loop,
	DWORD play_ms,
	int need_stereo_upmix)
{
	Mp3StreamHandle stream;
	unsigned long rate = 0;
	int channels = 0;
	FAudio *audio = NULL;
	FAudioMasteringVoice *master = NULL;
	FAudioSourceVoice *source = NULL;
	FAudioWaveFormatEx wfx;
	UINT32 hr;
	int started = 0;
	int decode_done = 0;
	int chunk_index = 0;
	struct LiveBuffer live[64];
	int live_count = 0;
	UINT64 total_samples = 0;
	DWORD t0;
	DWORD t_first_audio = 0;
	DWORD t_decode_done = 0;
	int i;

	memset(live, 0, sizeof(live));
	memset(&stream, 0, sizeof(stream));

	if (!mp3_stream_begin(&stream, file_data, file_len, &rate, &channels)) {
		fprintf(stderr, "mp3_stream_begin failed\n");
		return 1;
	}
	if (channels < 1) {
		channels = 1;
	}
	if (rate < 1) {
		rate = 44100;
	}
	if (need_stereo_upmix && channels == 1) {
		channels = 2;
	}

	printf("stream: %lu Hz, %d ch (decode while playing)\n", rate, channels);
	fflush(stdout);

	t0 = GetTickCount();

	hr = FAudioCreate(&audio, 0, FAUDIO_DEFAULT_PROCESSOR);
	if (FAILED(hr) || audio == NULL) {
		fprintf(stderr, "FAudioCreate failed: 0x%08x\n", (unsigned)hr);
		mp3_stream_end(&stream);
		return 1;
	}

	hr = FAudio_CreateMasteringVoice(audio, &master, 2, 44100, 0, 0, NULL);
	if (FAILED(hr) || master == NULL) {
		fprintf(stderr, "FAudio_CreateMasteringVoice failed: 0x%08x\n", (unsigned)hr);
		FAudio_Release(audio);
		mp3_stream_end(&stream);
		return 1;
	}

	fill_wfx(&wfx, (int)rate, channels, 16);
	hr = FAudio_CreateSourceVoice(audio, &source, &wfx, 0, 2.0f, NULL, NULL, NULL);
	if (FAILED(hr) || source == NULL) {
		fprintf(stderr, "FAudio_CreateSourceVoice failed: 0x%08x\n", (unsigned)hr);
		FAudioVoice_DestroyVoice(master);
		FAudio_Release(audio);
		mp3_stream_end(&stream);
		return 1;
	}

	apply_stereo_output_matrix(source);
	FAudioVoice_SetVolume(source, 1.0f, FAUDIO_COMMIT_NOW);

	while (!decode_done) {
		FAudioBuffer ab;
		unsigned char *chunk = NULL;
		unsigned long chunk_bytes = 0;
		unsigned char *play_pcm = NULL;
		unsigned long play_bytes = 0;
		int done = 0;

		if (!mp3_stream_read_chunk(&stream, &chunk, &chunk_bytes, &done)) {
			if (done) {
				decode_done = 1;
			}
			break;
		}
		decode_done = done;

		play_pcm = chunk;
		play_bytes = chunk_bytes;
		if (need_stereo_upmix && play_pcm != NULL && chunk_bytes >= 2) {
			if (!upmix_mono_chunk(chunk, chunk_bytes, &play_pcm, &play_bytes)) {
				mp3_decode_free(chunk);
				break;
			}
			mp3_decode_free(chunk);
		}

		memset(&ab, 0, sizeof(ab));
		ab.AudioBytes = play_bytes;
		ab.pAudioData = play_pcm;
		if (decode_done) {
			ab.Flags = FAUDIO_END_OF_STREAM;
		}

		hr = FAudioSourceVoice_SubmitSourceBuffer(source, &ab, NULL);
		if (FAILED(hr)) {
			fprintf(stderr, "SubmitSourceBuffer failed: 0x%08x\n", (unsigned)hr);
			mp3_decode_free(play_pcm);
			break;
		}

		if (live_count < 64) {
			live[live_count].pcm = play_pcm;
			live[live_count].pcm_bytes = play_bytes;
			live_count++;
		} else {
			mp3_decode_free(play_pcm);
		}

		total_samples += play_bytes / (unsigned long)wfx.nBlockAlign;
		chunk_index++;

		if (!started) {
			hr = FAudioSourceVoice_Start(source, 0, FAUDIO_COMMIT_NOW);
			if (FAILED(hr)) {
				fprintf(stderr, "Start failed: 0x%08x\n", (unsigned)hr);
				break;
			}
			started = 1;
			t_first_audio = GetTickCount();
			printf(
				"first audio at +%lu ms (chunk %d, %lu bytes)\n",
				(unsigned long)(t_first_audio - t0),
				chunk_index,
				play_bytes);
			fflush(stdout);
		}
	}

	mp3_stream_end(&stream);
	t_decode_done = GetTickCount();

	if (started) {
		printf(
			"all chunks submitted in %lu ms (%d buffers), first audio +%lu ms\n",
			(unsigned long)(t_decode_done - t0),
			chunk_index,
			(unsigned long)(t_first_audio ? (t_first_audio - t0) : 0));
		wait_until_done(source, total_samples, loop, play_ms);
	}

	FAudioSourceVoice_Stop(source, 0, FAUDIO_COMMIT_NOW);
	FAudioSourceVoice_FlushSourceBuffers(source);
	FAudioVoice_DestroyVoice(source);
	FAudioVoice_DestroyVoice(master);
	FAudio_Release(audio);

	for (i = 0; i < live_count; i++) {
		mp3_decode_free(live[i].pcm);
	}
	return 0;
}

static int play_preloaded(
	unsigned char *pcm,
	unsigned long pcm_bytes,
	unsigned long rate,
	int channels,
	unsigned long samples,
	int loop,
	DWORD play_ms)
{
	FAudio *audio = NULL;
	FAudioMasteringVoice *master = NULL;
	FAudioSourceVoice *source = NULL;
	FAudioWaveFormatEx wfx;
	FAudioBuffer ab;
	UINT32 hr;
	UINT64 total_samples = 0;
	double duration_sec = 0.0;

	total_samples = (UINT64)samples;
	duration_sec = (rate > 0) ? (double)total_samples / (double)rate : 0.0;

	printf(
		"decoded: %lu bytes PCM, %lu Hz, %d ch, %lu samples, %.2f s\n",
		pcm_bytes,
		rate,
		channels,
		samples,
		duration_sec);

	hr = FAudioCreate(&audio, 0, FAUDIO_DEFAULT_PROCESSOR);
	if (FAILED(hr) || audio == NULL) {
		fprintf(stderr, "FAudioCreate failed: 0x%08x\n", (unsigned)hr);
		return 1;
	}

	hr = FAudio_CreateMasteringVoice(audio, &master, 2, 44100, 0, 0, NULL);
	if (FAILED(hr) || master == NULL) {
		fprintf(stderr, "FAudio_CreateMasteringVoice failed: 0x%08x\n", (unsigned)hr);
		FAudio_Release(audio);
		return 1;
	}

	fill_wfx(&wfx, (int)rate, channels, 16);
	hr = FAudio_CreateSourceVoice(audio, &source, &wfx, 0, 2.0f, NULL, NULL, NULL);
	if (FAILED(hr) || source == NULL) {
		fprintf(stderr, "FAudio_CreateSourceVoice failed: 0x%08x\n", (unsigned)hr);
		FAudioVoice_DestroyVoice(master);
		FAudio_Release(audio);
		return 1;
	}

	memset(&ab, 0, sizeof(ab));
	ab.AudioBytes = pcm_bytes;
	ab.pAudioData = pcm;
	if (loop) {
		ab.LoopCount = FAUDIO_LOOP_INFINITE;
		ab.LoopLength = pcm_bytes / (unsigned long)wfx.nBlockAlign;
	} else {
		ab.LoopCount = 0;
	}

	hr = FAudioSourceVoice_SubmitSourceBuffer(source, &ab, NULL);
	if (FAILED(hr)) {
		fprintf(stderr, "SubmitSourceBuffer failed: 0x%08x\n", (unsigned)hr);
		FAudioVoice_DestroyVoice(source);
		FAudioVoice_DestroyVoice(master);
		FAudio_Release(audio);
		return 1;
	}

	FAudioVoice_SetVolume(source, 1.0f, FAUDIO_COMMIT_NOW);
	apply_stereo_output_matrix(source);

	hr = FAudioSourceVoice_Start(source, 0, FAUDIO_COMMIT_NOW);
	if (FAILED(hr)) {
		fprintf(stderr, "Start failed: 0x%08x\n", (unsigned)hr);
		FAudioVoice_DestroyVoice(source);
		FAudioVoice_DestroyVoice(master);
		FAudio_Release(audio);
		return 1;
	}

	if (loop && play_ms == 0) {
		printf("playing (loop forever, close window or Ctrl+C)...\n");
	} else if (play_ms > 0) {
		printf("playing %lu ms%s...\n", (unsigned long)play_ms, loop ? " (looped)" : "");
	} else {
		printf("playing once (%.2f s)...\n", duration_sec);
	}

	wait_until_done(source, total_samples, loop, play_ms);

	FAudioSourceVoice_Stop(source, 0, FAUDIO_COMMIT_NOW);
	FAudioSourceVoice_FlushSourceBuffers(source);
	FAudioVoice_DestroyVoice(source);
	FAudioVoice_DestroyVoice(master);
	FAudio_Release(audio);
	return 0;
}

static void print_usage(const char *argv0)
{
	fprintf(
		stderr,
		"Usage: %s <file.mp3> [--loop] [--seconds N] [--preload]\n"
		"  Default: stream decode to FAudio (starts quickly).\n"
		"  --preload: decode entire file before play (like game menu path).\n"
		"  --loop: requires --preload (single FAudio buffer loop).\n",
		argv0);
}

int main(int argc, char **argv)
{
	const char *path = NULL;
	int loop = 0;
	int preload = 0;
	DWORD play_ms = 0;
	unsigned char *file_data = NULL;
	unsigned long file_len = 0;
	unsigned char *pcm = NULL;
	unsigned long pcm_bytes = 0;
	unsigned long rate = 0;
	int channels = 0;
	unsigned long samples = 0;
	DWORD t0;
	DWORD t_decode = 0;
	int i;
	int rc;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--loop") == 0) {
			loop = 1;
		} else if (strcmp(argv[i], "--preload") == 0) {
			preload = 1;
		} else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
			play_ms = (DWORD)(atoi(argv[++i]) * 1000);
		} else if (path == NULL) {
			path = argv[i];
		} else {
			print_usage(argv[0]);
			return 2;
		}
	}

	if (path == NULL) {
		print_usage(argv[0]);
		return 2;
	}

	if (loop && !preload) {
		fprintf(
			stderr,
			"note: --loop uses full preload decode; adding --preload automatically.\n");
		preload = 1;
	}

	if (!load_file(path, &file_data, &file_len)) {
		return 1;
	}
	printf("loaded %s (%lu bytes)\n", path, file_len);
	fflush(stdout);

	if (!preload) {
		rc = play_streamed(file_data, file_len, loop, play_ms, 1);
		free(file_data);
		printf("done.\n");
		return rc;
	}

	t0 = GetTickCount();
	printf("decoding full file (--preload)...\n");
	fflush(stdout);

	if (!mp3_decode_to_pcm16(
			file_data,
			file_len,
			&pcm,
			&pcm_bytes,
			&rate,
			&channels,
			&samples))
	{
		fprintf(stderr, "mp3_decode_to_pcm16 failed\n");
		free(file_data);
		return 1;
	}
	free(file_data);
	file_data = NULL;

	t_decode = GetTickCount();
	printf("decode took %lu ms\n", (unsigned long)(t_decode - t0));
	fflush(stdout);

	if (channels < 1) {
		channels = 1;
	}
	if (rate < 1) {
		rate = 44100;
	}
	{
		unsigned char *mono = pcm;
		unsigned long mono_bytes = pcm_bytes;
		unsigned char *stereo = NULL;
		unsigned long stereo_bytes = 0;

		if (channels == 1) {
			if (!upmix_mono_chunk(mono, mono_bytes, &stereo, &stereo_bytes)) {
				fprintf(stderr, "mono upmix failed\n");
				mp3_decode_free(pcm);
				return 1;
			}
			mp3_decode_free(pcm);
			pcm = stereo;
			pcm_bytes = stereo_bytes;
			channels = 2;
			samples = pcm_bytes / 4;
		}
	}

	rc = play_preloaded(pcm, pcm_bytes, rate, channels, samples, loop, play_ms);
	mp3_decode_free(pcm);
	printf("done.\n");
	return rc;
}
