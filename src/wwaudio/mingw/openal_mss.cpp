/*
** OpenAL + audio_decode backend implementing the Miles (AIL) stub API.
*/

#include "mss_stub.h"
#include "audio_decode.h"

#include <AL/al.h>
#include <AL/alc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

static CRITICAL_SECTION g_lock;
static bool g_lock_ok = false;
static bool g_started = false;
static ALCdevice *g_device = NULL;
static ALCcontext *g_context = NULL;
static DIG_DRIVER_TYPE g_driver_data;
static HDIGDRIVER g_driver = &g_driver_data;
static HPROVIDER g_3d_provider = (HPROVIDER)2;
static bool g_3d_open = false;
static char g_last_error[256] = "OpenAL";

enum OalKind
{
	OAL_KIND_2D,
	OAL_KIND_3D,
	OAL_KIND_STREAM
};

struct OalVoice
{
	OalKind kind;
	ALuint source;
	ALuint buffer;
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
};

static void lock_init(void)
{
	if (!g_lock_ok) {
		InitializeCriticalSection(&g_lock);
		g_lock_ok = true;
	}
}

static void oal_lock(void)
{
	lock_init();
	EnterCriticalSection(&g_lock);
}

static void oal_unlock(void)
{
	LeaveCriticalSection(&g_lock);
}

static unsigned int read_le32(const unsigned char *p)
{
	return (unsigned int)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

static ALenum oal_format(S32 channels, S32 bits)
{
	if (bits == 16) {
		return (channels > 1) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
	}
	if (bits == 8) {
		return (channels > 1) ? AL_FORMAT_STEREO8 : AL_FORMAT_MONO8;
	}
	return AL_FORMAT_MONO16;
}

static void oal_clear_error(void)
{
	alGetError();
}

static bool oal_check(const char *where)
{
	ALenum err = alGetError();
	if (err == AL_NO_ERROR) {
		return true;
	}
	_snprintf(g_last_error, sizeof(g_last_error) - 1, "%s: AL error %d", where, (int)err);
	g_last_error[sizeof(g_last_error) - 1] = '\0';
	return false;
}

static OalVoice *voice_from_sample(HSAMPLE s)
{
	return (OalVoice *)s;
}

static OalVoice *voice_from_3d(H3DSAMPLE s)
{
	return (OalVoice *)s;
}

static OalVoice *voice_from_stream(HSTREAM s)
{
	return (OalVoice *)s;
}

static void voice_destroy(OalVoice *v)
{
	if (v == NULL) {
		return;
	}
	if (v->source != 0) {
		alSourceStop(v->source);
		alSourcei(v->source, AL_BUFFER, 0);
		alDeleteSources(1, &v->source);
	}
	if (v->buffer != 0) {
		alDeleteBuffers(1, &v->buffer);
	}
	audio_decode_free(v->pcm);
	delete v;
}

static bool voice_upload(OalVoice *v, const void *file_ptr, S32 file_len)
{
	AILSOUNDINFO info;
	unsigned char *pcm = NULL;
	unsigned long pcm_bytes = 0;

	if (v == NULL || file_ptr == NULL || file_len <= 0) {
		return false;
	}

	audio_decode_free(v->pcm);
	v->pcm = NULL;
	v->pcm_bytes = 0;

	if (!audio_decode_to_pcm(file_ptr, (unsigned long)file_len, &pcm, &pcm_bytes, &info)) {
		_snprintf(g_last_error, sizeof(g_last_error) - 1, "decode failed");
		return false;
	}

	v->pcm = pcm;
	v->pcm_bytes = pcm_bytes;
	v->rate = info.rate;
	v->channels = info.channels;
	v->bits = (info.bits > 0) ? info.bits : 16;

	alBufferData(
		v->buffer,
		oal_format(v->channels, v->bits),
		v->pcm,
		(ALsizei)v->pcm_bytes,
		(ALsizei)v->rate);
	if (!oal_check("alBufferData")) {
		return false;
	}

	alSourcei(v->source, AL_BUFFER, (ALint)v->buffer);
	alSourcef(v->source, AL_GAIN, v->volume);
	if (v->kind == OAL_KIND_2D || v->kind == OAL_KIND_STREAM) {
		alSourcei(v->source, AL_SOURCE_RELATIVE, AL_TRUE);
		alSource3f(v->source, AL_POSITION, v->pan, 0.0f, 0.0f);
	} else {
		alSourcei(v->source, AL_SOURCE_RELATIVE, AL_TRUE);
		alSourcef(v->source, AL_REFERENCE_DISTANCE, v->min_dist);
		alSourcef(v->source, AL_MAX_DISTANCE, v->max_dist);
	}
	/* Westwood INFINITE_LOOPS == 0 */
	alSourcei(v->source, AL_LOOPING, (v->loop_count == 0) ? AL_TRUE : AL_FALSE);
	return true;
}

static OalVoice *voice_allocate(OalKind kind)
{
	OalVoice *v = new OalVoice;
	if (v == NULL) {
		return NULL;
	}
	memset(v, 0, sizeof(*v));
	v->kind = kind;
	v->volume = 1.0f;
	v->min_dist = 1.0f;
	v->max_dist = 100.0f;
	v->loop_count = 1;
	alGenSources(1, &v->source);
	alGenBuffers(1, &v->buffer);
	if (v->source == 0 || v->buffer == 0) {
		voice_destroy(v);
		return NULL;
	}
	return v;
}

extern "C" {

void AIL_startup(void)
{
	oal_lock();
	if (!g_started) {
		g_device = alcOpenDevice(NULL);
		if (g_device != NULL) {
			g_context = alcCreateContext(g_device, NULL);
			if (g_context != NULL) {
				alcMakeContextCurrent(g_context);
				alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
				alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
				alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);
				{
					const ALfloat orient[6] = {0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f};
					alListenerfv(AL_ORIENTATION, orient);
				}
				g_started = true;
				g_driver_data.emulated_ds = FALSE;
				strcpy(g_last_error, "OK");
			}
		}
		if (!g_started) {
			strcpy(g_last_error, "alcOpenDevice failed");
		}
	}
	oal_unlock();
}

void AIL_shutdown(void)
{
	oal_lock();
	if (g_context != NULL) {
		alcMakeContextCurrent(NULL);
		alcDestroyContext(g_context);
		g_context = NULL;
	}
	if (g_device != NULL) {
		alcCloseDevice(g_device);
		g_device = NULL;
	}
	g_started = false;
	g_3d_open = false;
	oal_unlock();
}

void AIL_lock(void)
{
	oal_lock();
}

void AIL_unlock(void)
{
	oal_unlock();
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
	oal_clear_error();
	return (HSAMPLE)voice_allocate(OAL_KIND_2D);
}

void AIL_release_sample_handle(HSAMPLE sample)
{
	voice_destroy(voice_from_sample(sample));
}

void AIL_init_sample(HSAMPLE sample)
{
	OalVoice *v = voice_from_sample(sample);
	if (v == NULL) {
		return;
	}
	alSourceStop(v->source);
	alSourcei(v->source, AL_BUFFER, 0);
	v->volume = 1.0f;
	v->pan = 0.0f;
	v->loop_count = 1;
}

S32 AIL_set_named_sample_file(HSAMPLE sample, char *, void const *file_ptr, S32 file_len, S32)
{
	OalVoice *v = voice_from_sample(sample);
	if (v == NULL) {
		return 0;
	}
	return voice_upload(v, file_ptr, file_len) ? 1 : 0;
}

void AIL_start_sample(HSAMPLE sample)
{
	OalVoice *v = voice_from_sample(sample);
	if (v != NULL) {
		alSourcePlay(v->source);
	}
}

void AIL_stop_sample(HSAMPLE sample)
{
	OalVoice *v = voice_from_sample(sample);
	if (v != NULL) {
		alSourceStop(v->source);
	}
}

void AIL_resume_sample(HSAMPLE sample)
{
	AIL_start_sample(sample);
}

void AIL_end_sample(HSAMPLE sample)
{
	AIL_stop_sample(sample);
}

void AIL_set_sample_pan(HSAMPLE sample, F32 pan)
{
	OalVoice *v = voice_from_sample(sample);
	if (v != NULL) {
		v->pan = pan;
		alSource3f(v->source, AL_POSITION, pan, 0.0f, 0.0f);
	}
}

F32 AIL_sample_pan(HSAMPLE sample)
{
	OalVoice *v = voice_from_sample(sample);
	return (v != NULL) ? v->pan : 0.0f;
}

static float miles_volume_to_gain(F32 volume)
{
	if (volume <= 0.0f) {
		return 0.0f;
	}
	if (volume <= 127.0f) {
		return volume / 127.0f;
	}
	return 1.0f;
}

void AIL_set_sample_volume(HSAMPLE sample, F32 volume)
{
	OalVoice *v = voice_from_sample(sample);
	if (v != NULL) {
		v->volume = volume;
		alSourcef(v->source, AL_GAIN, miles_volume_to_gain(volume));
	}
}

F32 AIL_sample_volume(HSAMPLE sample)
{
	OalVoice *v = voice_from_sample(sample);
	return (v != NULL) ? v->volume : 1.0f;
}

void AIL_set_sample_loop_count(HSAMPLE sample, S32 count)
{
	OalVoice *v = voice_from_sample(sample);
	if (v != NULL) {
		v->loop_count = count;
		if (v->source != 0) {
			alSourcei(v->source, AL_LOOPING, (count == 0) ? AL_TRUE : AL_FALSE);
		}
	}
}

S32 AIL_sample_loop_count(HSAMPLE sample)
{
	OalVoice *v = voice_from_sample(sample);
	return (v != NULL) ? v->loop_count : 0;
}

void AIL_set_sample_ms_position(HSAMPLE sample, S32 ms)
{
	OalVoice *v = voice_from_sample(sample);
	if (v != NULL) {
		alSourcef(v->source, AL_SEC_OFFSET, ms / 1000.0f);
	}
}

void AIL_sample_ms_position(HSAMPLE sample, S32 *len, S32 *pos)
{
	OalVoice *v = voice_from_sample(sample);
	if (v == NULL) {
		if (len) {
			*len = 0;
		}
		if (pos) {
			*pos = 0;
		}
		return;
	}
	if (len) {
		*len = (S32)((v->pcm_bytes * 1000.0f) / (float)((v->rate * v->channels * (v->bits >> 3))));
	}
	if (pos) {
		float sec = 0.0f;
		alGetSourcef(v->source, AL_SEC_OFFSET, &sec);
		*pos = (S32)(sec * 1000.0f);
	}
}

void AIL_set_sample_user_data(HSAMPLE sample, U32 index, S32 data)
{
	OalVoice *v = voice_from_sample(sample);
	if (v != NULL && index < 8u) {
		v->user_data[index] = data;
	}
}

S32 AIL_sample_user_data(HSAMPLE sample, U32 index)
{
	OalVoice *v = voice_from_sample(sample);
	return (v != NULL && index < 8u) ? v->user_data[index] : 0;
}

S32 AIL_sample_playback_rate(HSAMPLE sample)
{
	OalVoice *v = voice_from_sample(sample);
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
	OalVoice *v = voice_from_sample(sample);
	unsigned char *file_data = NULL;
	unsigned long file_len = 0;

	if (v == NULL) {
		return NULL;
	}
	v->kind = OAL_KIND_STREAM;

	if (v->pcm_bytes == 0 && filename != NULL && filename[0] != '\0') {
		if (audio_load_file(filename, &file_data, &file_len)) {
			voice_upload(v, file_data, (S32)file_len);
			audio_decode_free(file_data);
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
	OalVoice *v = voice_from_stream(stream);
	if (v == NULL) {
		return;
	}
	if (onoff) {
		alSourcePause(v->source);
	} else {
		alSourcePlay(v->source);
	}
}

void AIL_close_stream(HSTREAM stream)
{
	AIL_stop_sample((HSAMPLE)stream);
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
		*name = (char *)"OpenAL Soft";
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
	return (H3DSAMPLE)voice_allocate(OAL_KIND_3D);
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
	OalVoice *v = voice_from_3d(sample);
	if (v == NULL || file_ptr == NULL || file_len <= 0) {
		return 0;
	}
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
	OalVoice *v = voice_from_3d(sample);
	if (v != NULL) {
		alSourcePlay(v->source);
	}
}

void AIL_stop_3D_sample(H3DSAMPLE sample)
{
	OalVoice *v = voice_from_3d(sample);
	if (v != NULL) {
		alSourceStop(v->source);
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
	OalVoice *v = voice_from_3d(sample);
	if (v != NULL) {
		v->volume = volume;
		alSourcef(v->source, AL_GAIN, volume);
	}
}

F32 AIL_3D_sample_volume(H3DSAMPLE sample)
{
	OalVoice *v = voice_from_3d(sample);
	return (v != NULL) ? v->volume : 1.0f;
}

void AIL_set_3D_sample_loop_count(H3DSAMPLE sample, S32 count)
{
	OalVoice *v = voice_from_3d(sample);
	if (v != NULL) {
		v->loop_count = count;
		if (v->source != 0) {
			alSourcei(v->source, AL_LOOPING, (count == 0) ? AL_TRUE : AL_FALSE);
		}
	}
}

S32 AIL_3D_sample_loop_count(H3DSAMPLE sample)
{
	OalVoice *v = voice_from_3d(sample);
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
	OalVoice *v = voice_from_3d(sample);
	return (v != NULL) ? v->pcm_bytes : 0;
}

void AIL_set_3D_object_user_data(H3DSAMPLE sample, U32 index, S32 data)
{
	OalVoice *v = voice_from_3d(sample);
	if (v != NULL && index < 8u) {
		v->user_data[index] = data;
	}
}

S32 AIL_3D_object_user_data(H3DSAMPLE sample, U32 index)
{
	OalVoice *v = voice_from_3d(sample);
	return (v != NULL && index < 8u) ? v->user_data[index] : 0;
}

S32 AIL_3D_sample_playback_rate(H3DSAMPLE sample)
{
	OalVoice *v = voice_from_3d(sample);
	return (v != NULL) ? (S32)v->rate : 44100;
}

void AIL_set_3D_sample_playback_rate(H3DSAMPLE, S32)
{
}

void AIL_set_3D_position(H3DSAMPLE sample, F32 x, F32 y, F32 z)
{
	OalVoice *v = voice_from_3d(sample);
	if (v != NULL) {
		alSource3f(v->source, AL_POSITION, x, y, z);
	}
}

void AIL_set_3D_orientation(H3DSAMPLE sample, F32 xf, F32 yf, F32 zf, F32 xu, F32 yu, F32 zu)
{
	OalVoice *v = voice_from_3d(sample);
	if (v != NULL) {
		alSource3f(v->source, AL_DIRECTION, xf, yf, zf);
		(void)xu;
		(void)yu;
		(void)zu;
	}
}

void AIL_set_3D_velocity_vector(H3DSAMPLE, F32, F32, F32)
{
}

void AIL_set_3D_sample_distances(H3DSAMPLE sample, F32 max_dist, F32 min_dist)
{
	OalVoice *v = voice_from_3d(sample);
	if (v != NULL) {
		v->min_dist = min_dist;
		v->max_dist = max_dist;
		alSourcef(v->source, AL_REFERENCE_DISTANCE, min_dist);
		alSourcef(v->source, AL_MAX_DISTANCE, max_dist);
	}
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
