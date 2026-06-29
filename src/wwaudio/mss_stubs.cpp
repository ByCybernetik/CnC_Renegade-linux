#include "mss_stub.h"

#include <string.h>
#include <time.h>
#include <pthread.h>
#include <map>

static char g_last_error[] = "Miles stub (no audio hardware)";

static HSAMPLE g_stub_sample = (HSAMPLE)1;
static H3DSAMPLE g_stub_3d_sample = (H3DSAMPLE)2;
static DIG_DRIVER_TYPE g_stub_driver_data;
static HDIGDRIVER g_stub_driver = &g_stub_driver_data;
static HPROVIDER g_stub_provider = (HPROVIDER)4;
static H3DPOBJECT g_stub_listener = (H3DPOBJECT)5;

/* Per-sample user data storage (8 slots each). The real MSS stores user data
 * per-sample; the original stub used a single global array, which meant one
 * handle's data corrupted all others. Use intptr_t so 64-bit callers can
 * store full pointers without truncation on LP64 (x86_64 Linux). */
struct UserData8 { intptr_t d[8]; };
static std::map<HSAMPLE, UserData8> g_sample_user_data;
static std::map<H3DSAMPLE, UserData8> g_3d_user_data;

void AIL_startup(void) {}
void AIL_shutdown(void) {}
void AIL_lock(void) {}
void AIL_unlock(void) {}

S32 AIL_set_preference(U32, S32) { return AIL_NO_ERROR; }
char *AIL_last_error(void) { return g_last_error; }

S32 AIL_waveOutOpen(HDIGDRIVER *driver, LPHWAVEOUT *, unsigned int, LPWAVEFORMAT, unsigned long)
{
	if (driver != NULL) {
		g_stub_driver_data.emulated_ds = FALSE;
		*driver = g_stub_driver;
	}
	return AIL_NO_ERROR;
}

void AIL_waveOutClose(HDIGDRIVER) {}

HSAMPLE AIL_allocate_sample_handle(HDIGDRIVER) { return g_stub_sample; }
void AIL_release_sample_handle(HSAMPLE) {}
void AIL_init_sample(HSAMPLE) {}
S32 AIL_set_named_sample_file(HSAMPLE, char *, void const *, S32, S32) { return 1; }
void AIL_start_sample(HSAMPLE) {}
void AIL_stop_sample(HSAMPLE) {}
void AIL_resume_sample(HSAMPLE) {}
void AIL_end_sample(HSAMPLE) {}
void AIL_set_sample_pan(HSAMPLE, F32) {}
F32 AIL_sample_pan(HSAMPLE) { return 0.0f; }
void AIL_set_sample_volume(HSAMPLE, F32) {}
F32 AIL_sample_volume(HSAMPLE) { return 1.0f; }
void AIL_set_sample_loop_count(HSAMPLE, S32) {}
S32 AIL_sample_loop_count(HSAMPLE) { return 0; }
void AIL_set_sample_ms_position(HSAMPLE, S32) {}
void AIL_sample_ms_position(HSAMPLE, S32 *len, S32 *pos)
{
	if (len) *len = 0;
	if (pos) *pos = 0;
}
void AIL_set_sample_user_data(HSAMPLE sample, U32 index, intptr_t data)
{
	if (index < 8u) {
		g_sample_user_data[sample].d[index] = data;
	}
}
intptr_t AIL_sample_user_data(HSAMPLE sample, U32 index)
{
	if (index < 8u) {
		std::map<HSAMPLE, UserData8>::iterator it = g_sample_user_data.find(sample);
		return (it != g_sample_user_data.end()) ? it->second.d[index] : 0;
	}
	return 0;
}
S32 AIL_sample_playback_busy(HSAMPLE) { return 0; }
S32 AIL_sample_playback_rate(HSAMPLE) { return 44100; }
void AIL_set_sample_playback_rate(HSAMPLE, S32) {}
void AIL_set_sample_processor(HSAMPLE, S32, HPROVIDER) {}
void AIL_set_filter_sample_preference(HSAMPLE, char const *, void const *) {}
void AIL_set_room_type(HDIGDRIVER, S32) {}
S32 AIL_room_type(HDIGDRIVER) { return ENVIRONMENT_GENERIC; }

HSTREAM AIL_open_stream_by_sample(HDIGDRIVER, HSAMPLE, char const *, S32) { return (HSTREAM)6; }
void AIL_start_stream(HSTREAM) {}
void AIL_pause_stream(HSTREAM, S32) {}
void AIL_close_stream(HSTREAM) {}
void AIL_set_stream_pan(HSTREAM, F32) {}
F32 AIL_stream_pan(HSTREAM) { return 0.0f; }
void AIL_set_stream_volume(HSTREAM, F32) {}
F32 AIL_stream_volume(HSTREAM) { return 1.0f; }
void AIL_set_stream_loop_block(HSTREAM, S32, S32) {}
void AIL_set_stream_loop_count(HSTREAM, S32) {}
S32 AIL_stream_loop_count(HSTREAM) { return 0; }
void AIL_set_stream_ms_position(HSTREAM, S32) {}
void AIL_stream_ms_position(HSTREAM, S32 *len, S32 *pos)
{
	if (len) *len = 0;
	if (pos) *pos = 0;
}
S32 AIL_stream_playback_rate(HSTREAM) { return 44100; }
void AIL_set_stream_playback_rate(HSTREAM, S32) {}

S32 AIL_enumerate_3D_providers(HPROENUM *, HPROVIDER *, char **) { return 0; }
S32 AIL_open_3D_provider(HPROVIDER provider)
{
	return (provider != NULL) ? M3D_NOERR : -1;
}
void AIL_close_3D_provider(HPROVIDER) {}
void AIL_set_3D_speaker_type(HPROVIDER, U32) {}

H3DSAMPLE AIL_allocate_3D_sample_handle(HPROVIDER) { return g_stub_3d_sample; }
void AIL_release_3D_sample_handle(H3DSAMPLE) {}
S32 AIL_set_3D_sample_file(H3DSAMPLE, void const *) { return 1; }
S32 AIL_set_3D_sample_file_len(H3DSAMPLE, void const *, S32) { return 1; }
void AIL_start_3D_sample(H3DSAMPLE) {}
void AIL_stop_3D_sample(H3DSAMPLE) {}
void AIL_resume_3D_sample(H3DSAMPLE) {}
void AIL_end_3D_sample(H3DSAMPLE) {}
void AIL_set_3D_sample_volume(H3DSAMPLE, F32) {}
F32 AIL_3D_sample_volume(H3DSAMPLE) { return 1.0f; }
void AIL_set_3D_sample_loop_count(H3DSAMPLE, S32) {}
S32 AIL_3D_sample_loop_count(H3DSAMPLE) { return 0; }
void AIL_set_3D_sample_offset(H3DSAMPLE, U32) {}
U32 AIL_3D_sample_offset(H3DSAMPLE) { return 0; }
U32 AIL_3D_sample_length(H3DSAMPLE) { return 0; }
void AIL_set_3D_object_user_data(H3DSAMPLE sample, U32 index, intptr_t data)
{
	if (index < 8u) {
		g_3d_user_data[sample].d[index] = data;
	}
}
intptr_t AIL_3D_object_user_data(H3DSAMPLE sample, U32 index)
{
	if (index < 8u) {
		std::map<H3DSAMPLE, UserData8>::iterator it = g_3d_user_data.find(sample);
		return (it != g_3d_user_data.end()) ? it->second.d[index] : 0;
	}
	return 0;
}
S32 AIL_3D_sample_playback_rate(H3DSAMPLE) { return 44100; }
void AIL_set_3D_sample_playback_rate(H3DSAMPLE, S32) {}

void AIL_set_3D_position(H3DSAMPLE, F32, F32, F32) {}
void AIL_set_3D_orientation(H3DSAMPLE, F32, F32, F32, F32, F32, F32) {}
void AIL_set_3D_velocity_vector(H3DSAMPLE, F32, F32, F32) {}
void AIL_set_3D_sample_distances(H3DSAMPLE, F32, F32) {}
void AIL_set_3D_sample_effects_level(H3DSAMPLE, F32) {}

H3DPOBJECT AIL_3D_open_listener(HPROVIDER) { return g_stub_listener; }

S32 AIL_enumerate_filters(HPROENUM *, HPROVIDER *dest, char **)
{
	if (dest) {
		*dest = g_stub_provider;
	}
	return 0;
}

void AIL_set_file_callbacks(AIL_FILE_OPEN_CALLBACK, AIL_FILE_CLOSE_CALLBACK,
	AIL_FILE_SEEK_CALLBACK, AIL_FILE_READ_CALLBACK)
{
}

void AIL_stop_timer(HTIMER) {}
void AIL_release_timer_handle(HTIMER) {}

S32 AIL_WAV_info(void const *, AILSOUNDINFO *info)
{
	if (info != NULL) {
		memset(info, 0, sizeof(*info));
		info->format = WAVE_FORMAT_PCM;
		info->rate = 22050;
		info->bits = 16;
		info->channels = 1;
	}
	return 0;
}

/* ---- Win32 API stubs for Linux (only _beginthread, others provided by platform) ---- */

/* _beginthread: spawn a detached pthread */
uintptr_t _beginthread(void (*proc)(void*), unsigned, void* arg)
{
	pthread_t thread;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&thread, &attr, (void*(*)(void*))proc, arg);
	pthread_attr_destroy(&attr);
	return (uintptr_t)thread;
}
