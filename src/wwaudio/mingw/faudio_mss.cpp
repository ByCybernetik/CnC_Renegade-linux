/*
** FAudio (XAudio2) + audio_decode backend implementing the Miles (AIL) stub API.
*/

#include "mss_stub.h"
#include "audio_decode.h"

#include <FAudio.h>
#include <F3DAudio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>


#ifndef FAUDIO_COMMIT_ALL
#define FAUDIO_COMMIT_ALL 0
#endif

static CRITICAL_SECTION g_lock;
static bool g_lock_ok = false;
static bool g_started = false;
static FAudio *g_audio = NULL;
static FAudioMasteringVoice *g_master = NULL;
static F3DAUDIO_HANDLE g_f3d;
static bool g_f3d_ok = false;
static DIG_DRIVER_TYPE g_driver_data;
static HDIGDRIVER g_driver = &g_driver_data;
static HPROVIDER g_3d_provider = (HPROVIDER)2;
static bool g_3d_open = false;
static char g_last_error[256] = "FAudio";

enum FaKind
{
	FA_KIND_2D,
	FA_KIND_3D,
	FA_KIND_STREAM
};

struct FaVoice
{
	FaKind kind;
	FAudioSourceVoice *source;
	FAudioWaveFormatEx wfx;
	unsigned char *pcm;
	unsigned long pcm_bytes;
	U32 rate;
	S32 channels;
	S32 bits;
	F32 volume;
	F32 pan;
	S32 loop_count;
	S32 user_data[8];
	F32 min_dist;
	F32 max_dist;
	F3DAUDIO_EMITTER emitter;
	float matrix[16];
	char stream_path[260];
	DWORD last_levels_ms;
	bool pcm_submitted;
	/* Wine/FAudio: BuffersQueued often 0 while audio still audible — track wall-clock play window. */
	DWORD playback_start_ms;
	DWORD playback_end_ms;
};

static DWORD voice_now_ms(void)
{
	return (DWORD)GetTickCount();
}

static void lock_init(void)
{
	if (!g_lock_ok) {
		InitializeCriticalSection(&g_lock);
		g_lock_ok = true;
	}
}

static void fa_lock(void)
{
	lock_init();
	EnterCriticalSection(&g_lock);
}

static void fa_unlock(void)
{
	LeaveCriticalSection(&g_lock);
}

static unsigned int read_le32(const unsigned char *p)
{
	return (unsigned int)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

static void fill_wfx(FAudioWaveFormatEx *wfx, S32 rate, S32 channels, S32 bits)
{
	memset(wfx, 0, sizeof(*wfx));
	wfx->wFormatTag = 1; /* WAVE_FORMAT_PCM */
	wfx->nChannels = (WORD)((channels > 0) ? channels : 1);
	wfx->nSamplesPerSec = (DWORD)((rate > 0) ? rate : 44100);
	wfx->wBitsPerSample = (WORD)((bits > 0) ? bits : 16);
	wfx->nBlockAlign = (WORD)(wfx->nChannels * (wfx->wBitsPerSample / 8));
	wfx->nAvgBytesPerSec = wfx->nSamplesPerSec * wfx->nBlockAlign;
	/* Plain PCM: cbSize must be 0 (WAVEFORMATEX / XAudio2), not sizeof(struct). */
	wfx->cbSize = 0;
}

static S32 faudio_infer_channels(unsigned long pcm_bytes, S32 reported, S32 bits)
{
	S32 ch = (reported > 0) ? reported : 1;
	unsigned long bytes_per_sample;

	if (bits <= 0) {
		bits = 16;
	}
	bytes_per_sample = (unsigned long)(bits / 8);
	if (bytes_per_sample == 0) {
		bytes_per_sample = 2;
	}

	while (ch > 1 && pcm_bytes % (ch * bytes_per_sample) != 0) {
		ch--;
	}
	if (ch < 1) {
		ch = 1;
	}
	return ch;
}

static void fa_set_error(const char *where, uint32_t hr)
{
	_snprintf(g_last_error, sizeof(g_last_error) - 1, "%s: FAudio 0x%08lX", where, (unsigned long)hr);
	g_last_error[sizeof(g_last_error) - 1] = '\0';
}

static FaVoice *voice_from_sample(HSAMPLE s)
{
	return (FaVoice *)s;
}

static FaVoice *voice_from_3d(H3DSAMPLE s)
{
	return (FaVoice *)s;
}

static FaVoice *voice_from_stream(HSTREAM s)
{
	return (FaVoice *)s;
}

static float miles_volume_to_gain(F32 volume)
{
	if (volume <= 0.0f) {
		return 0.0f;
	}
	/* Miles sample volume is 0..127 (passed as int promoted to float). */
	if (volume <= 127.0f) {
		return volume / 127.0f;
	}
	return 1.0f;
}

static float miles_pan_to_balance(F32 pan)
{
	/* Miles pan 0..127 from AIL_set_*_pan / SoundStreamHandle (not normalized 0..1). */
	float balance;

	if (pan >= 0.0f && pan <= 127.0f) {
		balance = (pan - 63.0f) / 63.0f;
	} else {
		balance = pan;
	}
	if (balance < -1.0f) {
		return -1.0f;
	}
	if (balance > 1.0f) {
		return 1.0f;
	}
	return balance;
}

static float voice_pan_gain_scale(FaVoice *v)
{
	float balance;
	float left;
	float right;

	if (v == NULL) {
		return 1.0f;
	}

	balance = miles_pan_to_balance(v->pan);
	if (balance < -1.0f) {
		balance = -1.0f;
	}
	if (balance > 1.0f) {
		balance = 1.0f;
	}

	left = 1.0f;
	right = 1.0f;
	if (balance < 0.0f) {
		right = 1.0f + balance;
	} else if (balance > 0.0f) {
		left = 1.0f - balance;
	}

	/* Approximate stereo pan with a single master gain (avoids SetOutputMatrix/SetChannelVolumes). */
	return (left + right) * 0.5f;
}

static void voice_apply_levels(FaVoice *v)
{
	float gain;
	float milesg;

	if (v == NULL || v->source == NULL) {
		return;
	}

	milesg = miles_volume_to_gain(v->volume);
	gain = milesg * voice_pan_gain_scale(v);

	FAudioVoice_SetVolume(v->source, gain, FAUDIO_COMMIT_NOW);
	v->last_levels_ms = voice_now_ms();
}

static void voice_apply_levels_throttled(FaVoice *v, int force)
{
	DWORD now;

	if (v == NULL || v->source == NULL) {
		return;
	}

	now = voice_now_ms();
	if (!force && v->last_levels_ms != 0 && (now - v->last_levels_ms) < 33) {
		return;
	}

	voice_apply_levels(v);
}

static void voice_apply_3d(FaVoice *v)
{
	F3DAUDIO_LISTENER listener;
	F3DAUDIO_DSP_SETTINGS dsp;

	if (v == NULL || v->source == NULL || !g_f3d_ok || v->kind != FA_KIND_3D) {
		return;
	}

	memset(&listener, 0, sizeof(listener));
	listener.OrientFront.z = -1.0f;
	listener.OrientTop.y = 1.0f;

	memset(&dsp, 0, sizeof(dsp));
	dsp.pMatrixCoefficients = v->matrix;
	dsp.SrcChannelCount = (v->wfx.nChannels > 0) ? v->wfx.nChannels : 1;
	dsp.DstChannelCount = 2;
	if (dsp.SrcChannelCount == 0 || dsp.DstChannelCount == 0) {
		return;
	}

	v->emitter.ChannelCount = dsp.SrcChannelCount;
	F3DAudioCalculate(g_f3d, &listener, &v->emitter, F3DAUDIO_CALCULATE_MATRIX, &dsp);
	if (dsp.pMatrixCoefficients == NULL) {
		return;
	}
	FAudioVoice_SetOutputMatrix(
		v->source,
		NULL,
		dsp.SrcChannelCount,
		dsp.DstChannelCount,
		v->matrix,
		FAUDIO_COMMIT_NOW);
}

static unsigned long voice_pcm_samples(FaVoice *v, unsigned long pcm_bytes)
{
	unsigned long block_align = (unsigned long)v->wfx.nBlockAlign;

	if (block_align == 0) {
		block_align = (unsigned long)((v->channels > 0) ? v->channels : 1);
		block_align *= (unsigned long)((v->bits > 0) ? (v->bits / 8) : 2);
	}
	if (block_align == 0) {
		block_align = 1;
	}
	return pcm_bytes / block_align;
}

static DWORD voice_playback_duration_ms(const FaVoice *v)
{
	unsigned long bytes_per_sec;

	if (v == NULL || v->pcm_bytes == 0) {
		return 0;
	}

	bytes_per_sec = (unsigned long)v->rate * (unsigned long)v->channels * (unsigned long)(v->bits / 8);
	if (bytes_per_sec == 0) {
		bytes_per_sec = (unsigned long)v->wfx.nAvgBytesPerSec;
	}
	if (bytes_per_sec == 0) {
		return 0;
	}

	return (DWORD)((v->pcm_bytes * 1000UL) / bytes_per_sec) + 80;
}

static int voice_source_is_playing(FaVoice *v)
{
	FAudioVoiceState state;
	unsigned long total_samples;
	DWORD now;

	if (v == NULL) {
		return 0;
	}

	now = voice_now_ms();
	if (v->playback_start_ms != 0 && now >= v->playback_start_ms && now < v->playback_end_ms) {
		return 1;
	}

	if (v->source == NULL) {
		return 0;
	}

	memset(&state, 0, sizeof(state));
	FAudioSourceVoice_GetState(v->source, &state, 0);
	if (state.BuffersQueued > 0) {
		return 1;
	}

	total_samples = voice_pcm_samples(v, v->pcm_bytes);
	if (total_samples > 0 && state.SamplesPlayed > 0 &&
		state.SamplesPlayed < (UINT64)total_samples)
	{
		return 1;
	}

	return 0;
}

static int voice_source_is_busy(FaVoice *v)
{
	return voice_source_is_playing(v);
}

static void voice_fill_buffer_loop(FaVoice *v, FAudioBuffer *ab, unsigned long pcm_bytes)
{
	unsigned long loop_samples;

	memset(ab, 0, sizeof(*ab));
	ab->AudioBytes = pcm_bytes;
	ab->pAudioData = v->pcm;
	loop_samples = voice_pcm_samples(v, pcm_bytes);

	/* Westwood: loop_count 0 (INFINITE_LOOPS) = loop forever; 1 = play once.
	 * FAudio LoopLength is in samples (frames), not bytes. */
	if (v->loop_count == 0) {
		ab->LoopCount = FAUDIO_LOOP_INFINITE;
		ab->LoopLength = loop_samples;
	} else if (v->loop_count > 1) {
		ab->LoopCount = (UINT32)v->loop_count;
		ab->LoopLength = loop_samples;
	} else {
		ab->LoopCount = 0;
		ab->LoopBegin = 0;
		ab->LoopLength = 0;
	}
}

static bool voice_recreate_source(FaVoice *v)
{
	uint32_t hr;

	if (v == NULL || v->pcm == NULL || v->pcm_bytes == 0) {
		return false;
	}
	if (v->source != NULL) {
		return true;
	}

	hr = FAudio_CreateSourceVoice(g_audio, &v->source, &v->wfx, 0, 2.0f, NULL, NULL, NULL);
	if (FAILED(hr) || v->source == NULL) {
		fa_set_error("FAudio_CreateSourceVoice", hr);
		return false;
	}
	return true;
}

static void voice_teardown_source(FaVoice *v);

static bool voice_resubmit(FaVoice *v)
{
	FAudioBuffer ab;
	uint32_t hr;

	if (v == NULL || v->pcm == NULL || v->pcm_bytes == 0) {
		return false;
	}
	if (!voice_recreate_source(v)) {
		return false;
	}

	FAudioSourceVoice_Stop(v->source, 0, FAUDIO_COMMIT_NOW);
	FAudioSourceVoice_FlushSourceBuffers(v->source);
	voice_fill_buffer_loop(v, &ab, v->pcm_bytes);

	hr = FAudioSourceVoice_SubmitSourceBuffer(v->source, &ab, NULL);
	if (FAILED(hr)) {
		fa_set_error("FAudioSourceVoice_SubmitSourceBuffer", hr);
		return false;
	}

	voice_apply_levels(v);
	v->pcm_submitted = true;
	return true;
}

static bool voice_restart_playback(FaVoice *v)
{
	FAudioVoiceState state;
	uint32_t hr;

	if (v == NULL || v->pcm_bytes == 0) {
		return false;
	}
	if (!voice_recreate_source(v)) {
		return false;
	}

	memset(&state, 0, sizeof(state));
	FAudioSourceVoice_GetState(v->source, &state, 0);

	/*
	** README: buffer already submitted in voice_upload — only Start, never re-submit
	** when BuffersQueued==0 (re-submit caused silence / stacked buffers).
	** Collapse stacked buffers with full source teardown.
	*/
	if (state.BuffersQueued > 1) {
		voice_teardown_source(v);
		v->pcm_submitted = false;
		if (!voice_resubmit(v)) {
			return false;
		}
	} else if (state.BuffersQueued == 0 && v->pcm_submitted) {
		if (v->kind == FA_KIND_STREAM) {
			if (!voice_resubmit(v)) {
				return false;
			}
		}
	}


	hr = FAudioSourceVoice_Start(v->source, 0, FAUDIO_COMMIT_NOW);
	if (SUCCEEDED(hr)) {
		DWORD duration_ms = voice_playback_duration_ms(v);

		v->playback_start_ms = voice_now_ms();
		v->playback_end_ms = v->playback_start_ms + duration_ms;
	}
	if (FAILED(hr)) {
		fa_set_error("FAudioSourceVoice_Start", hr);
		return false;
	}
	return true;
}

static void voice_teardown_source(FaVoice *v)
{
	if (v == NULL || v->source == NULL ) {
		return;
	}

	FAudioSourceVoice_Stop(v->source, 0, FAUDIO_COMMIT_NOW);
	FAudioSourceVoice_FlushSourceBuffers(v->source);
	FAudioVoice_DestroyVoice(v->source);
	v->source = NULL;
	v->playback_start_ms = 0;
	v->playback_end_ms = 0;
}

static void voice_clear_playback(FaVoice *v)
{
	if (v == NULL) {
		return;
	}

	/* Stop FAudio before freeing PCM — SubmitSourceBuffer keeps our pointer until drain. */
	voice_teardown_source(v);
	audio_decode_free(v->pcm);
	v->pcm = NULL;
	v->pcm_bytes = 0;
	v->pcm_submitted = false;
	v->stream_path[0] = '\0';
	v->playback_start_ms = 0;
	v->playback_end_ms = 0;
}

/* One-shot SFX (reload, UI click): avoid FA_KIND_STREAM playback quirks on Wine/FAudio. */
static int voice_is_oneshot_sfx(const FaVoice *v)
{
	return (v != NULL && v->loop_count == 1 && v->pcm_bytes > 0 &&
		v->pcm_bytes <= 262144);
}

static void voice_destroy(FaVoice *v)
{
	if (v == NULL) {
		return;
	}
	voice_clear_playback(v);
	delete v;
}

static int voice_upmix_mono_pcm(unsigned char **pcm, unsigned long *pcm_bytes)
{
	unsigned long sample_count;
	unsigned char *out;
	unsigned short *src;
	unsigned short *dst;
	unsigned long i;

	if (pcm == NULL || pcm_bytes == NULL || *pcm == NULL || *pcm_bytes < 2) {
		return 0;
	}

	sample_count = *pcm_bytes / 2;
	out = (unsigned char *)malloc(*pcm_bytes * 2);
	if (out == NULL) {
		return 0;
	}

	src = (unsigned short *)*pcm;
	dst = (unsigned short *)out;
	for (i = 0; i < sample_count; i++) {
		dst[i * 2] = src[i];
		dst[i * 2 + 1] = src[i];
	}

	free(*pcm);
	*pcm = out;
	*pcm_bytes *= 2;
	return 1;
}

static bool voice_upload(FaVoice *v, const void *file_ptr, S32 file_len)
{
	AILSOUNDINFO info;
	unsigned char *pcm = NULL;
	unsigned long pcm_bytes = 0;
	FAudioBuffer ab;
	uint32_t hr;
	bool ok;

	if (v == NULL || !g_started) {
		return false;
	}

	fa_lock();

	if (file_ptr == NULL || file_len <= 0) {
		fa_unlock();
		return false;
	}


	voice_teardown_source(v);
	audio_decode_free(v->pcm);
	v->pcm = NULL;
	v->pcm_bytes = 0;
	v->pcm_submitted = false;

	if (!audio_decode_to_pcm(file_ptr, (unsigned long)file_len, &pcm, &pcm_bytes, &info)) {
		_snprintf(g_last_error, sizeof(g_last_error) - 1, "decode failed");
		fa_unlock();
		return false;
	}

	v->pcm = pcm;
	v->pcm_bytes = pcm_bytes;
	v->rate = info.rate;
	v->bits = 16;
	v->channels = faudio_infer_channels(pcm_bytes, info.channels, v->bits);
	/* Trim stereo PCM to whole frames (bad infer causes scratchy surface SFX). */
	if (v->channels == 2 && v->pcm_bytes >= 4) {
		v->pcm_bytes &= ~(unsigned long)3;
	}
	/* Mono 2D/stream: upmix to stereo (Wine/FAudio mono voices are often inaudible). */
	if (v->channels == 1 && v->kind != FA_KIND_3D) {
		if (!voice_upmix_mono_pcm(&v->pcm, &v->pcm_bytes)) {
			audio_decode_free(v->pcm);
			v->pcm = NULL;
			fa_unlock();
			return false;
		}
		v->channels = 2;
	}
	fill_wfx(&v->wfx, v->rate, v->channels, v->bits);
	/* Long streams (menu.mp3 ~16 MiB PCM): use infinite loop on first submit.
	 * Re-submitting huge buffers after Set_Loop_Count fails on some FAudio builds. */
	if (v->kind == FA_KIND_STREAM && pcm_bytes > 500000) {
		v->loop_count = 0;
	}

	hr = FAudio_CreateSourceVoice(g_audio, &v->source, &v->wfx, 0, 2.0f, NULL, NULL, NULL);
	if (FAILED(hr) || v->source == NULL) {
		fa_set_error("FAudio_CreateSourceVoice", hr);
		fa_unlock();
		return false;
	}

	voice_fill_buffer_loop(v, &ab, v->pcm_bytes);

	hr = FAudioSourceVoice_SubmitSourceBuffer(v->source, &ab, NULL);
	if (FAILED(hr)) {
		fa_set_error("FAudioSourceVoice_SubmitSourceBuffer", hr);
		fa_unlock();
		return false;
	}

	if (v->kind == FA_KIND_3D) {
		voice_apply_3d(v);
	}
	voice_apply_levels(v);
	v->pcm_submitted = true;
	ok = true;
	fa_unlock();
	return ok;
}

static bool voice_load_stream_file(FaVoice *v, const char *filename)
{
	unsigned char *file_data = NULL;
	unsigned long file_len = 0;
	int load_ok;
	int upload_ok;

	if (v == NULL || filename == NULL || filename[0] == '\0') {
		return false;
	}

	if (v->pcm != NULL && v->pcm_bytes > 0 && v->stream_path[0] != '\0' &&
		strcmp(v->stream_path, filename) == 0 &&
		v->loop_count != 1)
	{
		return true;
	}

	load_ok = audio_load_file(filename, &file_data, &file_len);
	upload_ok = 0;
	if (load_ok) {
		upload_ok = voice_upload(v, file_data, (S32)file_len) ? 1 : 0;
		audio_decode_free(file_data);
	}

	if (upload_ok) {
		strncpy(v->stream_path, filename, sizeof(v->stream_path) - 1);
		v->stream_path[sizeof(v->stream_path) - 1] = '\0';
	} else {
		v->stream_path[0] = '\0';
	}

	return upload_ok != 0;
}

static FaVoice *voice_allocate(FaKind kind)
{
	FaVoice *v = new FaVoice;
	if (v == NULL) {
		return NULL;
	}
	memset(v, 0, sizeof(*v));
	v->kind = kind;
	v->stream_path[0] = '\0';
	v->volume = 1.0f;
	v->pan = 63.0f;
	v->min_dist = 1.0f;
	v->max_dist = 100.0f;
	v->loop_count = 1;
	v->emitter.ChannelCount = 1;
	return v;
}

extern "C" {

void AIL_startup(void)
{
	uint32_t hr;

	fa_lock();
	if (!g_started) {
		hr = FAudioCreate(&g_audio, 0, FAUDIO_DEFAULT_PROCESSOR);
		if (SUCCEEDED(hr) && g_audio != NULL) {
			hr = FAudio_CreateMasteringVoice(
				g_audio,
				&g_master,
				2,
				44100,
				0,
				0,
				NULL);
			if (SUCCEEDED(hr) && g_master != NULL) {
				F3DAudioInitialize(SPEAKER_STEREO, 343.5f, g_f3d);
				g_f3d_ok = true;
				g_started = true;
				g_driver_data.emulated_ds = FALSE;
				strcpy(g_last_error, "OK");
			} else {
				fa_set_error("FAudio_CreateMasteringVoice", hr);
			}
		} else {
			fa_set_error("FAudioCreate", hr);
		}
	}
	fa_unlock();
}

void AIL_shutdown(void)
{
	fa_lock();
	if (g_master != NULL) {
		FAudioVoice_DestroyVoice(g_master);
		g_master = NULL;
	}
	if (g_audio != NULL) {
		FAudio_Release(g_audio);
		g_audio = NULL;
	}
	g_f3d_ok = false;
	g_started = false;
	g_3d_open = false;
	fa_unlock();
}

void AIL_lock(void)
{
	fa_lock();
}

void AIL_unlock(void)
{
	fa_unlock();
}

S32 AIL_set_preference(U32, S32 value)
{
	return value;
}

char *AIL_last_error(void)
{
	return g_last_error;
}

S32 AIL_waveOutOpen(HDIGDRIVER *driver, LPHWAVEOUT *, unsigned int, LPWAVEFORMAT, unsigned long)
{
	if (driver != NULL) {
		*driver = g_driver;
	}
	return g_started ? AIL_NO_ERROR : -1;
}

void AIL_waveOutClose(HDIGDRIVER)
{
}

HSAMPLE AIL_allocate_sample_handle(HDIGDRIVER)
{
	if (!g_started) {
		return NULL;
	}
	return (HSAMPLE)voice_allocate(FA_KIND_2D);
}

void AIL_release_sample_handle(HSAMPLE sample)
{
	voice_destroy(voice_from_sample(sample));
}

void AIL_init_sample(HSAMPLE sample)
{
	FaVoice *v = voice_from_sample(sample);

	if (v == NULL) {
		return;
	}

	/*
	** Pooled HSAMPLE handles keep FaVoice across Stop/End; discard stale PCM/voice
	** before loading the next file (reload after gunshot was distorted noise).
	*/
	voice_clear_playback(v);
	v->kind = FA_KIND_2D;
	v->stream_path[0] = '\0';
	v->pcm_submitted = false;
	v->volume = 1.0f;
	v->pan = 63.0f;
	v->loop_count = 1;
	v->last_levels_ms = 0;
}

S32 AIL_set_named_sample_file(HSAMPLE sample, char *, void const *file_ptr, S32 file_len, S32)
{
	FaVoice *v = voice_from_sample(sample);
	int ok;
	if (v == NULL) {
		return 0;
	}
	v->kind = FA_KIND_2D;
	v->stream_path[0] = '\0';
	ok = voice_upload(v, file_ptr, file_len) ? 1 : 0;
	return ok;
}

void AIL_start_sample(HSAMPLE sample)
{
	FaVoice *v = voice_from_sample(sample);

	fa_lock();
	if (v != NULL && v->source != NULL && v->pcm_bytes > 0) {
		FAudioVoiceState state;
		float milesg;
		float gain;

		memset(&state, 0, sizeof(state));
		FAudioSourceVoice_GetState(v->source, &state, 0);

		/*
		** Pooled handle reuse: Start on drained buffer = scratch/noise. Re-submit PCM
		** for one-shots before restart (reload after gunshot).
		*/
		if (voice_is_oneshot_sfx(v) &&
			(state.BuffersQueued == 0 || state.SamplesPlayed > 0))
		{
			v->pcm_submitted = false;
			voice_resubmit(v);
			memset(&state, 0, sizeof(state));
			FAudioSourceVoice_GetState(v->source, &state, 0);
		}

		voice_restart_playback(v);
		voice_apply_levels(v);

		/* Logs: menu 14380 upload_ok + restart but no gain_* — verify Start state. */
		if (v->kind == FA_KIND_2D && v->pcm_bytes < 262144) {
			milesg = miles_volume_to_gain(v->volume);
			gain = milesg * voice_pan_gain_scale(v);
			memset(&state, 0, sizeof(state));
			FAudioSourceVoice_GetState(v->source, &state, 0);
		}
	}
	fa_unlock();
}

void AIL_stop_sample(HSAMPLE sample)
{
	FaVoice *v = voice_from_sample(sample);
	if (v != NULL && v->source != NULL) {
		FAudioSourceVoice_Stop(v->source, 0, FAUDIO_COMMIT_NOW);
	}
}

void AIL_resume_sample(HSAMPLE sample)
{
	FaVoice *v = voice_from_sample(sample);
	if (v != NULL && v->source != NULL) {
		FAudioSourceVoice_Start(v->source, 0, FAUDIO_COMMIT_NOW);
	}
}

void AIL_end_sample(HSAMPLE sample)
{
	FaVoice *v = voice_from_sample(sample);

	AIL_stop_sample(sample);
	voice_clear_playback(v);
}

S32 AIL_sample_playback_busy(HSAMPLE sample)
{
	FaVoice *v = voice_from_sample(sample);

	if (v == NULL) {
		return 0;
	}
	return voice_source_is_playing(v) ? 1 : 0;
}

void AIL_set_sample_pan(HSAMPLE sample, F32 pan)
{
	FaVoice *v = voice_from_sample(sample);

	fa_lock();
	if (v != NULL) {
		v->pan = pan;
	}
	fa_unlock();
}

F32 AIL_sample_pan(HSAMPLE sample)
{
	FaVoice *v = voice_from_sample(sample);
	return (v != NULL) ? v->pan : 0.0f;
}

void AIL_set_sample_volume(HSAMPLE sample, F32 volume)
{
	FaVoice *v = voice_from_sample(sample);

	fa_lock();
	if (v != NULL) {
		v->volume = volume;
		voice_apply_levels_throttled(v, 0);
	}
	fa_unlock();
}

F32 AIL_sample_volume(HSAMPLE sample)
{
	FaVoice *v = voice_from_sample(sample);
	return (v != NULL) ? v->volume : 1.0f;
}

void AIL_set_sample_loop_count(HSAMPLE sample, S32 count)
{
	FaVoice *v = voice_from_sample(sample);
	if (v != NULL) {
		if (v->loop_count == count) {
			return;
		}
		v->loop_count = count;
		if (v->source != NULL && v->pcm != NULL && v->pcm_bytes <= 500000) {
			voice_resubmit(v);
		}
	}
}

S32 AIL_sample_loop_count(HSAMPLE sample)
{
	FaVoice *v = voice_from_sample(sample);
	return (v != NULL) ? v->loop_count : 0;
}

void AIL_set_sample_ms_position(HSAMPLE sample, S32 ms)
{
	FaVoice *v = voice_from_sample(sample);

	if (v == NULL || v->source == NULL || v->pcm_bytes == 0) {
		return;
	}

	/*
	** Weapon fire (first-person 2D) calls Seek(0) before each shot to restart the sample.
	** Miles rewinds the buffer; we must re-submit PCM (see weapons.cpp Do_Firing_Effects).
	*/
	if (ms <= 0 && v->kind != FA_KIND_STREAM) {
		FAudioVoiceState state;

		/*
		** Only rewind mid-playback. Seek(0) on a freshly submitted buffer (SamplesPlayed==0)
		** stacked extra buffers (BuffersQueued=3) and distorted reload / surface SFX.
		** Never resubmit huge stream/music PCM (breaks background music).
		*/
		memset(&state, 0, sizeof(state));
		FAudioSourceVoice_GetState(v->source, &state, 0);
		if (state.SamplesPlayed > 0) {
			v->pcm_submitted = false;
			voice_resubmit(v);
		}
	}
}

void AIL_sample_ms_position(HSAMPLE sample, S32 *len, S32 *pos)
{
	FaVoice *v = voice_from_sample(sample);
	FAudioVoiceState state;
	S32 bytes_per_sec;

	if (v == NULL) {
		if (len) {
			*len = 0;
		}
		if (pos) {
			*pos = 0;
		}
		return;
	}

	bytes_per_sec = (S32)(v->rate * v->channels * (v->bits >> 3));
	if (len) {
		*len = (bytes_per_sec > 0)
			? (S32)((v->pcm_bytes * 1000) / (unsigned long)bytes_per_sec)
			: 0;
	}
	if (pos && v->source != NULL) {
		memset(&state, 0, sizeof(state));
		FAudioSourceVoice_GetState(v->source, &state, 0);
		*pos = (bytes_per_sec > 0)
			? (S32)((state.SamplesPlayed * 1000) / (UINT64)v->rate)
			: 0;
	} else if (pos) {
		*pos = 0;
	}
}

void AIL_set_sample_user_data(HSAMPLE sample, U32 index, S32 data)
{
	FaVoice *v = voice_from_sample(sample);
	if (v != NULL && index < 8u) {
		v->user_data[index] = data;
	}
}

S32 AIL_sample_user_data(HSAMPLE sample, U32 index)
{
	FaVoice *v = voice_from_sample(sample);
	return (v != NULL && index < 8u) ? v->user_data[index] : 0;
}

S32 AIL_sample_playback_rate(HSAMPLE sample)
{
	FaVoice *v = voice_from_sample(sample);
	return (v != NULL) ? (S32)v->rate : 44100;
}

void AIL_set_sample_playback_rate(HSAMPLE, S32)
{
}

void AIL_set_sample_processor(HSAMPLE, S32, HPROVIDER)
{
}

void AIL_set_filter_sample_preference(HSAMPLE, char const *, void const *)
{
}

void AIL_set_room_type(HDIGDRIVER, S32)
{
}

S32 AIL_room_type(HDIGDRIVER)
{
	return ENVIRONMENT_GENERIC;
}

HSTREAM AIL_open_stream_by_sample(HDIGDRIVER, HSAMPLE sample, char const *filename, S32)
{
	FaVoice *v = voice_from_sample(sample);
	int loaded = 0;

	if (v == NULL) {
		return NULL;
	}

	voice_clear_playback(v);
	v->kind = FA_KIND_STREAM;
	v->loop_count = 1;

	if (filename != NULL && filename[0] != '\0') {
		loaded = voice_load_stream_file(v, filename) ? 1 : 0;
		/*
		** Decoded reload/UI-sized clips: play as 2D sample, not STREAM (logs OK but audible noise).
		*/
		if (loaded && voice_is_oneshot_sfx(v)) {
			v->kind = FA_KIND_2D;
		}
		if (!loaded) {
			_snprintf(
				g_last_error,
				sizeof(g_last_error) - 1,
				"stream open failed: %s",
				filename);
			return NULL;
		}
	}
	return (HSTREAM)v;
}

void AIL_start_stream(HSTREAM stream)
{
	AIL_start_sample((HSAMPLE)stream);
}

void AIL_pause_stream(HSTREAM stream, S32 onoff)
{
	FaVoice *v = voice_from_stream(stream);
	if (v == NULL || v->source == NULL) {
		return;
	}
	if (onoff) {
		FAudioSourceVoice_Stop(v->source, 0, FAUDIO_COMMIT_NOW);
	} else {
		AIL_resume_sample((HSAMPLE)stream);
	}
}

void AIL_close_stream(HSTREAM stream)
{
	FaVoice *v = voice_from_stream(stream);
	if (v != NULL && v->source != NULL) {
		FAudioSourceVoice_Stop(v->source, 0, FAUDIO_COMMIT_NOW);
		FAudioSourceVoice_FlushSourceBuffers(v->source);
	}
}

void AIL_set_stream_pan(HSTREAM stream, F32 pan)
{
	AIL_set_sample_pan((HSAMPLE)stream, pan);
}

F32 AIL_stream_pan(HSTREAM stream)
{
	return AIL_sample_pan((HSAMPLE)stream);
}

void AIL_set_stream_volume(HSTREAM stream, F32 volume)
{
	AIL_set_sample_volume((HSAMPLE)stream, volume);
}

F32 AIL_stream_volume(HSTREAM stream)
{
	return AIL_sample_volume((HSAMPLE)stream);
}

void AIL_set_stream_loop_block(HSTREAM, S32, S32)
{
}

void AIL_set_stream_loop_count(HSTREAM stream, S32 count)
{
	AIL_set_sample_loop_count((HSAMPLE)stream, count);
}

S32 AIL_stream_loop_count(HSTREAM stream)
{
	return AIL_sample_loop_count((HSAMPLE)stream);
}

void AIL_set_stream_ms_position(HSTREAM stream, S32 ms)
{
	AIL_set_sample_ms_position((HSAMPLE)stream, ms);
}

void AIL_stream_ms_position(HSTREAM stream, S32 *len, S32 *pos)
{
	AIL_sample_ms_position((HSAMPLE)stream, len, pos);
}

S32 AIL_stream_playback_rate(HSTREAM stream)
{
	return AIL_sample_playback_rate((HSAMPLE)stream);
}

void AIL_set_stream_playback_rate(HSTREAM stream, S32 rate)
{
	AIL_set_sample_playback_rate((HSAMPLE)stream, rate);
}

S32 AIL_enumerate_3D_providers(HPROENUM *next, HPROVIDER *dest, char **name)
{
	if (next == NULL || *next != HPROENUM_FIRST) {
		return 0;
	}
	if (dest != NULL) {
		*dest = g_3d_provider;
	}
	if (name != NULL) {
		*name = (char *)"FAudio";
	}
	*next = (HPROENUM)0;
	return 1;
}

S32 AIL_open_3D_provider(HPROVIDER provider)
{
	if (provider == g_3d_provider && g_started) {
		g_3d_open = true;
		return M3D_NOERR;
	}
	return -1;
}

void AIL_close_3D_provider(HPROVIDER)
{
	g_3d_open = false;
}

void AIL_set_3D_speaker_type(HPROVIDER, U32)
{
}

H3DSAMPLE AIL_allocate_3D_sample_handle(HPROVIDER provider)
{
	if (!g_3d_open || provider != g_3d_provider) {
		return NULL;
	}
	return (H3DSAMPLE)voice_allocate(FA_KIND_3D);
}

void AIL_release_3D_sample_handle(H3DSAMPLE sample)
{
	voice_destroy(voice_from_3d(sample));
}

static unsigned long infer_file_len(const void *file_ptr)
{
	const unsigned char *d = (const unsigned char *)file_ptr;

	if (d == NULL) {
		return 0;
	}
	if (memcmp(d, "RIFF", 4) == 0) {
		return read_le32(d + 4) + 8u;
	}
	return 0;
}

S32 AIL_set_3D_sample_file_len(H3DSAMPLE sample, void const *file_ptr, S32 file_len)
{
	FaVoice *v = voice_from_3d(sample);
	if (v == NULL || file_ptr == NULL || file_len <= 0) {
		return 0;
	}
	v->kind = FA_KIND_3D;
	return voice_upload(v, file_ptr, file_len) ? 1 : 0;
}

S32 AIL_set_3D_sample_file(H3DSAMPLE sample, void const *file_ptr)
{
	unsigned long len = infer_file_len(file_ptr);
	if (len == 0) {
		return 0;
	}
	return AIL_set_3D_sample_file_len(sample, file_ptr, (S32)len);
}

void AIL_start_3D_sample(H3DSAMPLE sample)
{
	FaVoice *v = voice_from_3d(sample);
	if (v != NULL && v->source != NULL && v->pcm_bytes > 0) {
		voice_restart_playback(v);
	}
}

void AIL_stop_3D_sample(H3DSAMPLE sample)
{
	FaVoice *v = voice_from_3d(sample);
	if (v != NULL && v->source != NULL) {
		FAudioSourceVoice_Stop(v->source, 0, FAUDIO_COMMIT_NOW);
	}
}

void AIL_resume_3D_sample(H3DSAMPLE sample)
{
	AIL_start_3D_sample(sample);
}

void AIL_end_3D_sample(H3DSAMPLE sample)
{
	AIL_stop_3D_sample(sample);
}

void AIL_set_3D_sample_volume(H3DSAMPLE sample, F32 volume)
{
	FaVoice *v = voice_from_3d(sample);

	fa_lock();
	if (v != NULL) {
		v->volume = volume;
		voice_apply_levels_throttled(v, 0);
	}
	fa_unlock();
}

F32 AIL_3D_sample_volume(H3DSAMPLE sample)
{
	FaVoice *v = voice_from_3d(sample);
	return (v != NULL) ? v->volume : 1.0f;
}

void AIL_set_3D_sample_loop_count(H3DSAMPLE sample, S32 count)
{
	FaVoice *v = voice_from_3d(sample);
	if (v != NULL) {
		v->loop_count = count;
		if (v->source != NULL && v->pcm != NULL && v->pcm_bytes <= 500000) {
			voice_resubmit(v);
		}
	}
}

S32 AIL_3D_sample_loop_count(H3DSAMPLE sample)
{
	FaVoice *v = voice_from_3d(sample);
	return (v != NULL) ? v->loop_count : 0;
}

void AIL_set_3D_sample_offset(H3DSAMPLE sample, U32)
{
	(void)sample;
}

U32 AIL_3D_sample_offset(H3DSAMPLE sample)
{
	(void)sample;
	return 0;
}

U32 AIL_3D_sample_length(H3DSAMPLE sample)
{
	FaVoice *v = voice_from_3d(sample);
	return (v != NULL) ? v->pcm_bytes : 0;
}

void AIL_set_3D_object_user_data(H3DSAMPLE sample, U32 index, S32 data)
{
	FaVoice *v = voice_from_3d(sample);
	if (v != NULL && index < 8u) {
		v->user_data[index] = data;
	}
}

S32 AIL_3D_object_user_data(H3DSAMPLE sample, U32 index)
{
	FaVoice *v = voice_from_3d(sample);
	return (v != NULL && index < 8u) ? v->user_data[index] : 0;
}

S32 AIL_3D_sample_playback_rate(H3DSAMPLE sample)
{
	FaVoice *v = voice_from_3d(sample);
	return (v != NULL) ? (S32)v->rate : 44100;
}

void AIL_set_3D_sample_playback_rate(H3DSAMPLE, S32)
{
}

void AIL_set_3D_position(H3DSAMPLE sample, F32 x, F32 y, F32 z)
{
	FaVoice *v = voice_from_3d(sample);

	fa_lock();
	if (v != NULL && v->source != NULL) {
		v->emitter.Position.x = x;
		v->emitter.Position.y = y;
		v->emitter.Position.z = z;
		voice_apply_3d(v);
	}
	fa_unlock();
}

void AIL_set_3D_orientation(H3DSAMPLE sample, F32 xf, F32 yf, F32 zf, F32 xu, F32 yu, F32 zu)
{
	FaVoice *v = voice_from_3d(sample);

	fa_lock();
	if (v != NULL && v->source != NULL) {
		v->emitter.OrientFront.x = xf;
		v->emitter.OrientFront.y = yf;
		v->emitter.OrientFront.z = zf;
		v->emitter.OrientTop.x = xu;
		v->emitter.OrientTop.y = yu;
		v->emitter.OrientTop.z = zu;
		voice_apply_3d(v);
	}
	fa_unlock();
}

void AIL_set_3D_velocity_vector(H3DSAMPLE, F32, F32, F32)
{
}

void AIL_set_3D_sample_distances(H3DSAMPLE sample, F32 max_dist, F32 min_dist)
{
	FaVoice *v = voice_from_3d(sample);

	fa_lock();
	if (v != NULL && v->source != NULL) {
		v->min_dist = min_dist;
		v->max_dist = max_dist;
		voice_apply_3d(v);
	}
	fa_unlock();
}

void AIL_set_3D_sample_effects_level(H3DSAMPLE, F32)
{
}

H3DPOBJECT AIL_3D_open_listener(HPROVIDER provider)
{
	(void)provider;
	return (H3DPOBJECT)1;
}

S32 AIL_enumerate_filters(HPROENUM *next, HPROVIDER *dest, char **)
{
	if (next != NULL && *next == HPROENUM_FIRST) {
		if (dest != NULL) {
			*dest = (HPROVIDER)3;
		}
		*next = (HPROENUM)0;
		return 1;
	}
	return 0;
}

void AIL_set_file_callbacks(
	AIL_FILE_OPEN_CALLBACK open_fn,
	AIL_FILE_CLOSE_CALLBACK close_fn,
	AIL_FILE_SEEK_CALLBACK seek_fn,
	AIL_FILE_READ_CALLBACK read_fn)
{
	audio_set_miles_file_callbacks(
		(audio_file_open_fn)open_fn,
		(audio_file_close_fn)close_fn,
		(audio_file_seek_fn)seek_fn,
		(audio_file_read_fn)read_fn);
}

void AIL_stop_timer(HTIMER)
{
}

void AIL_release_timer_handle(HTIMER)
{
}

S32 AIL_WAV_info(void const *data, AILSOUNDINFO *info)
{
	if (data == NULL || info == NULL) {
		return 0;
	}
	return audio_decode_probe(data, 0xffffffff, info) ? 1 : 0;
}

} /* extern "C" */
