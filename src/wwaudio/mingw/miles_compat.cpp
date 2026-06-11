/*
** Renegade legacy AIL API compatibility layer on top of Miles Sound System 9.3 (mss32.dll).
** WWAudio still calls the old H3DSAMPLE / HPROVIDER entry points from mss_stub.h.
*/

#include "miles_sdk.h"

#ifndef AILCALL
#define AILCALL __stdcall
#endif

#include "miles_win32_util.h"

#include <string.h>
#include <stdint.h>

struct RenDigDriverPub
{
	BOOL emulated_ds;
};

struct MilesDigWrapper
{
	RenDigDriverPub pub;
	HDIGDRIVER real;
};

typedef HSAMPLE RenH3DSAMPLE;
typedef void *RenH3DPOBJECT;
typedef S32 HPROENUM;
#define HPROENUM_FIRST ((HPROENUM)-1)
#define M3D_NOERR 0
#define AIL_NO_ERROR 0
#define ENVIRONMENT_GENERIC 0

typedef HSAMPLE (__stdcall *MilesAllocateSampleFn)(HDIGDRIVER dig);
typedef S32 (__stdcall *MilesInitSampleFn)(HSAMPLE sample, S32 format);
typedef void (__stdcall *MilesSetRoomTypeFn)(HDIGDRIVER dig, S32 room_type);
typedef void (__stdcall *MilesGetRoomTypeFn)(HDIGDRIVER dig, S32 *room_type);

static MilesDigWrapper g_dig = {};
static HPROVIDER g_3d_provider = (HPROVIDER)(UINTa)0x4d353030;
static bool g_3d_open = false;
static bool g_redist_set = false;
static HMODULE g_mss_module = NULL;
static MilesAllocateSampleFn g_real_allocate_sample = NULL;
static MilesInitSampleFn g_real_init_sample = NULL;
static MilesSetRoomTypeFn g_real_set_room_type = NULL;
static MilesGetRoomTypeFn g_real_get_room_type = NULL;

static void miles_set_redist_directory(void)
{
	char path[MAX_PATH];
	char *slash;

	if (g_redist_set) {
		return;
	}

	if (GetModuleFileNameA(NULL, path, sizeof(path)) == 0) {
		return;
	}

	slash = strrchr(path, '\\');
	if (slash == NULL) {
		slash = strrchr(path, '/');
	}
	if (slash != NULL) {
		*slash = '\0';
	}

	AIL_set_redist_directory(path);
	g_redist_set = true;
}

static void miles_bind_real(void)
{
	if (g_mss_module != NULL) {
		return;
	}

	miles_set_redist_directory();
	g_mss_module = GetModuleHandleA("mss32.dll");
	if (g_mss_module == NULL) {
		g_mss_module = LoadLibraryA("mss32.dll");
	}
	if (g_mss_module == NULL) {
		return;
	}

	g_real_allocate_sample = (MilesAllocateSampleFn)miles_get_proc(
		g_mss_module, "AIL_allocate_sample_handle", "_AIL_allocate_sample_handle@4");
	g_real_init_sample = (MilesInitSampleFn)miles_get_proc(
		g_mss_module, "AIL_init_sample", "_AIL_init_sample@8");
	g_real_set_room_type = (MilesSetRoomTypeFn)miles_get_proc(
		g_mss_module, "AIL_set_room_type", "_AIL_set_room_type@8");
	g_real_get_room_type = (MilesGetRoomTypeFn)miles_get_proc(
		g_mss_module, "AIL_room_type", "_AIL_room_type@8");
}

static HDIGDRIVER miles_real_dig(HDIGDRIVER driver)
{
	MilesDigWrapper *wrap;

	if (driver == NULL) {
		return g_dig.real;
	}

	wrap = (MilesDigWrapper *)((char *)driver - (size_t)&((MilesDigWrapper *)0)->pub);
	if (wrap == &g_dig) {
		return wrap->real;
	}

	return driver;
}

static void miles_parse_format(LPWAVEFORMAT format, U32 *rate, S32 *bits, S32 *channels)
{
	const WAVEFORMATEX *wfx = (const WAVEFORMATEX *)format;

	*rate = 44100;
	*bits = 16;
	*channels = 2;

	if (wfx == NULL) {
		return;
	}

	*rate = wfx->nSamplesPerSec;
	*bits = wfx->wBitsPerSample;
	*channels = wfx->nChannels;
	if (*bits <= 0) {
		*bits = 16;
	}
	if (*channels <= 0) {
		*channels = 2;
	}
	if (*rate <= 0) {
		*rate = 44100;
	}
}

static S32 miles_open_dig(LPWAVEFORMAT format)
{
	U32 rate;
	S32 bits;
	S32 channels;

	if (g_dig.real != NULL) {
		return AIL_NO_ERROR;
	}

	miles_set_redist_directory();
	miles_parse_format(format, &rate, &bits, &channels);

	/*
	** MSS defaults: 256 ms DS ring + 48 ms mix-ahead (~300 ms audible latency).
	** Tighten for SFX sync under Wine/native DS.
	*/
	AIL_set_preference(DIG_DS_FRAGMENT_SIZE, 1);
	AIL_set_preference(DIG_DS_FRAGMENT_CNT, 8);
	AIL_set_preference(DIG_DS_MIX_FRAGMENT_CNT, 4);
	AIL_set_preference(DIG_OUTPUT_BUFFER_SIZE, 8192);

	g_dig.real = AIL_open_digital_driver(rate, bits, channels, 0);
	if (g_dig.real == NULL) {
		return -1;
	}

	g_dig.pub.emulated_ds = FALSE;
	return AIL_NO_ERROR;
}

static HSAMPLE miles_to_sample(RenH3DSAMPLE sample)
{
	return (HSAMPLE)sample;
}

static RenH3DSAMPLE miles_to_3d(HSAMPLE sample)
{
	return (RenH3DSAMPLE)sample;
}

#define MILES_SAMPLE_STREAM_MAX 32
static HSAMPLE g_sample_streams[MILES_SAMPLE_STREAM_MAX];
static int g_sample_stream_count = 0;

static void miles_register_sample_stream(HSAMPLE sample)
{
	int i;

	if (sample == NULL) {
		return;
	}

	for (i = 0; i < g_sample_stream_count; i ++) {
		if (g_sample_streams[i] == sample) {
			return;
		}
	}

	if (g_sample_stream_count < MILES_SAMPLE_STREAM_MAX) {
		g_sample_streams[g_sample_stream_count ++] = sample;
	}
}

static void miles_unregister_sample_stream(HSTREAM stream)
{
	HSAMPLE sample = (HSAMPLE)stream;
	int i;

	for (i = 0; i < g_sample_stream_count; i ++) {
		if (g_sample_streams[i] == sample) {
			g_sample_streams[i] = g_sample_streams[-- g_sample_stream_count];
			return;
		}
	}
}

static bool miles_is_sample_stream(HSTREAM stream)
{
	HSAMPLE sample = (HSAMPLE)stream;
	int i;

	for (i = 0; i < g_sample_stream_count; i ++) {
		if (g_sample_streams[i] == sample) {
			return true;
		}
	}

	return false;
}

static HSAMPLE miles_stream_sample(HSTREAM stream)
{
	if (stream == NULL || stream == (HSTREAM)(UINTa)-1) {
		return NULL;
	}

	if (miles_is_sample_stream(stream)) {
		return (HSAMPLE)stream;
	}

	return AIL_stream_sample_handle(stream);
}

static void miles_reset_listener_3d(void)
{
	if (g_dig.real == NULL) {
		return;
	}

	/*
	** Renegade transforms sources into listener-space (Sound3DClass::Update_Miles_Transform);
	** keep the MSS listener at the origin with a fixed facing.
	*/
	AIL_set_listener_3D_position(g_dig.real, 0.0f, 0.0f, 0.0f);
	AIL_set_listener_3D_orientation(g_dig.real, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f);
}

static float miles_volume_to_gain(F32 volume)
{
	if (volume <= 0.0f) {
		return 0.0f;
	}
	/* WWAudio passes legacy Miles sample volume as 0..127. */
	if (volume <= 127.0f) {
		return volume / 127.0f;
	}
	return 1.0f;
}

static float miles_gain_to_volume(F32 gain)
{
	if (gain <= 0.0f) {
		return 0.0f;
	}
	if (gain <= 1.0f) {
		return gain * 127.0f;
	}
	return 127.0f;
}

static float miles_pan_to_mss(F32 pan)
{
	if (pan >= 0.0f && pan <= 127.0f) {
		return pan / 127.0f;
	}
	if (pan < 0.0f) {
		return 0.0f;
	}
	if (pan > 1.0f) {
		return 1.0f;
	}
	return pan;
}

static float mss_pan_to_miles(F32 pan)
{
	if (pan >= 0.0f && pan <= 1.0f) {
		return pan * 127.0f;
	}
	return pan;
}

extern "C" {

S32 AILCALL AIL_waveOutOpen(HDIGDRIVER *driver, LPHWAVEOUT *waveout, unsigned int device_id, LPWAVEFORMAT format, unsigned long flags)
{
	S32 result;

	(void)waveout;
	(void)device_id;
	(void)flags;

	result = miles_open_dig(format);
	if (driver != NULL) {
		*driver = (result == AIL_NO_ERROR) ? (HDIGDRIVER)(uintptr_t)&g_dig.pub : NULL;
	}
	return result;
}

void AILCALL AIL_waveOutClose(HDIGDRIVER driver)
{
	(void)driver;

	if (g_dig.real != NULL) {
		AIL_close_digital_driver(g_dig.real);
		g_dig.real = NULL;
	}
	g_dig.pub.emulated_ds = FALSE;
	g_3d_open = false;
}

HSAMPLE AILCALL AIL_allocate_sample_handle(HDIGDRIVER driver)
{
	miles_bind_real();
	if (g_real_allocate_sample == NULL) {
		return NULL;
	}
	return g_real_allocate_sample(miles_real_dig(driver));
}

void AILCALL AIL_init_sample(HSAMPLE sample)
{
	miles_bind_real();
	if (g_real_init_sample != NULL) {
		g_real_init_sample(sample, DIG_F_STEREO_16);
	}
}

S32 AILCALL AIL_sample_playback_busy(HSAMPLE sample)
{
	U32 status;

	if (sample == NULL) {
		return 0;
	}

	status = AIL_sample_status(sample);

	/*
	** Match original Miles semantics: released/stopped/done voices are idle.
	** After pause-menu Pause()->Free_Miles_Handle()->End_Sample, MSS often keeps
	** SMP_PLAYING without SMP_DONE; SMP_STOPPED / SMP_PLAYINGBUTRELEASED must
	** not block Get_*_Sample from reusing the voice.
	*/
	if ((status & (SMP_FREE | SMP_DONE | SMP_STOPPED | SMP_PLAYINGBUTRELEASED)) != 0) {
		return 0;
	}
	if ((status & SMP_PLAYING) == 0) {
		return 0;
	}

	/*
	** Wine/MSS can keep SMP_PLAYING after PCM ends; trust ms position so UI pool
	** handles recycle promptly (logs: ui_busy_skipped=4 while new UI sound waits).
	*/
	S32 len_ms = 0;
	S32 pos_ms = 0;
	AIL_sample_ms_position(sample, &len_ms, &pos_ms);
	if (len_ms > 0 && pos_ms >= len_ms) {
		return 0;
	}

	return 1;
}

void AILCALL AIL_set_sample_pan(HSAMPLE sample, F32 pan)
{
	F32 current_gain = 1.0f;
	F32 current_pan = 0.5f;

	AIL_sample_volume_pan(sample, &current_gain, &current_pan);
	AIL_set_sample_volume_pan(sample, current_gain, miles_pan_to_mss(pan));
}

F32 AILCALL AIL_sample_pan(HSAMPLE sample)
{
	F32 volume = 1.0f;
	F32 pan = 0.5f;

	AIL_sample_volume_pan(sample, &volume, &pan);
	return mss_pan_to_miles(pan);
}

void AILCALL AIL_set_sample_volume(HSAMPLE sample, F32 volume)
{
	F32 current_gain = 1.0f;
	F32 pan = 0.5f;

	AIL_sample_volume_pan(sample, &current_gain, &pan);
	AIL_set_sample_volume_pan(sample, miles_volume_to_gain(volume), pan);
}

F32 AILCALL AIL_sample_volume(HSAMPLE sample)
{
	F32 volume = 1.0f;
	F32 pan = 0.5f;

	AIL_sample_volume_pan(sample, &volume, &pan);
	return miles_gain_to_volume(volume);
}

/*
** Renegade opens streams on pooled HSAMPLE handles (Miles 6 API). MSS 9 only has
** AIL_open_stream() which allocates its own sample and crashes when that pointer
** is treated as a legacy stream handle. Load into the pooled voice instead.
*/
HSTREAM AILCALL AIL_open_stream_by_sample(
	HDIGDRIVER driver,
	HSAMPLE sample,
	char const *filename,
	S32 stream_mem)
{
	HDIGDRIVER dig;

	(void)stream_mem;

	if (sample == NULL || sample == (HSAMPLE)(UINTa)-1) {
		return NULL;
	}
	if (filename == NULL || filename[0] == '\0') {
		return NULL;
	}

	dig = miles_real_dig(driver);
	if (dig == NULL) {
		return NULL;
	}

	(void)dig;
	AIL_init_sample(sample);
	if (!AIL_set_named_sample_file(sample, (char *)filename, NULL, 0, -1)) {
		return NULL;
	}

	miles_register_sample_stream(sample);
	return (HSTREAM)sample;
}

void AILCALL AIL_start_stream(HSTREAM stream)
{
	HSAMPLE sample = miles_stream_sample(stream);
	if (sample != NULL) {
		AIL_start_sample(sample);
	}
}

void AILCALL AIL_pause_stream(HSTREAM stream, S32 onoff)
{
	HSAMPLE sample = miles_stream_sample(stream);
	if (sample == NULL) {
		return;
	}

	if (onoff) {
		AIL_stop_sample(sample);
	} else {
		AIL_resume_sample(sample);
	}
}

void AILCALL AIL_close_stream(HSTREAM stream)
{
	HSAMPLE sample = miles_stream_sample(stream);
	if (sample == NULL) {
		return;
	}

	if (miles_is_sample_stream(stream)) {
		AIL_end_sample(sample);
		miles_unregister_sample_stream(stream);
		return;
	}

	AIL_end_sample(sample);
}

void AILCALL AIL_set_stream_pan(HSTREAM stream, F32 pan)
{
	HSAMPLE sample = miles_stream_sample(stream);
	if (sample != NULL) {
		AIL_set_sample_pan(sample, pan);
	}
}

F32 AILCALL AIL_stream_pan(HSTREAM stream)
{
	HSAMPLE sample = miles_stream_sample(stream);
	if (sample != NULL) {
		return AIL_sample_pan(sample);
	}
	return 0.0f;
}

void AILCALL AIL_set_stream_volume(HSTREAM stream, F32 volume)
{
	HSAMPLE sample = miles_stream_sample(stream);
	if (sample != NULL) {
		AIL_set_sample_volume(sample, volume);
	}
}

F32 AILCALL AIL_stream_volume(HSTREAM stream)
{
	HSAMPLE sample = miles_stream_sample(stream);
	if (sample != NULL) {
		return AIL_sample_volume(sample);
	}
	return 0.0f;
}

void AILCALL AIL_set_stream_loop_block(HSTREAM stream, S32 start, S32 end)
{
	(void)start;
	(void)end;
	(void)stream;
}

void AILCALL AIL_set_stream_loop_count(HSTREAM stream, S32 count)
{
	HSAMPLE sample = miles_stream_sample(stream);
	if (sample != NULL) {
		AIL_set_sample_loop_count(sample, count);
	}
}

S32 AILCALL AIL_stream_loop_count(HSTREAM stream)
{
	HSAMPLE sample = miles_stream_sample(stream);
	if (sample != NULL) {
		return AIL_sample_loop_count(sample);
	}
	return 0;
}

void AILCALL AIL_set_stream_ms_position(HSTREAM stream, S32 ms)
{
	HSAMPLE sample = miles_stream_sample(stream);
	if (sample != NULL) {
		AIL_set_sample_ms_position(sample, ms);
	}
}

void AILCALL AIL_stream_ms_position(HSTREAM stream, S32 *len, S32 *pos)
{
	HSAMPLE sample = miles_stream_sample(stream);
	if (sample != NULL) {
		AIL_sample_ms_position(sample, len, pos);
	}
}

S32 AILCALL AIL_stream_playback_rate(HSTREAM stream)
{
	HSAMPLE sample = miles_stream_sample(stream);
	if (sample != NULL) {
		return AIL_sample_playback_rate(sample);
	}
	return 0;
}

void AILCALL AIL_set_stream_playback_rate(HSTREAM stream, S32 rate)
{
	HSAMPLE sample = miles_stream_sample(stream);
	if (sample != NULL) {
		AIL_set_sample_playback_rate(sample, rate);
	}
}

void AILCALL AIL_set_filter_sample_preference(HSAMPLE sample, char const *name, void const *val)
{
	AIL_filter_sample_property(sample, name, NULL, val, NULL);
}

S32 AILCALL AIL_enumerate_3D_providers(HPROENUM *next, HPROVIDER *dest, char **name)
{
	if (next == NULL || *next != HPROENUM_FIRST) {
		return 0;
	}

	if (dest != NULL) {
		*dest = g_3d_provider;
	}
	if (name != NULL) {
		*name = (char *)"Miles DirectSound3D";
	}
	*next = (HPROENUM)0;
	return 1;
}

S32 AILCALL AIL_open_3D_provider(HPROVIDER provider)
{
	if (provider == g_3d_provider && g_dig.real != NULL) {
		g_3d_open = true;
		miles_reset_listener_3d();
		return M3D_NOERR;
	}
	return -1;
}

void AILCALL AIL_close_3D_provider(HPROVIDER provider)
{
	(void)provider;
	g_3d_open = false;
}

void AILCALL AIL_set_3D_speaker_type(HPROVIDER provider, U32 speaker_type)
{
	(void)provider;
	(void)speaker_type;
}

RenH3DSAMPLE AILCALL AIL_allocate_3D_sample_handle(HPROVIDER provider)
{
	HSAMPLE sample;

	if (!g_3d_open || provider != g_3d_provider || g_dig.real == NULL) {
		return NULL;
	}

	sample = AIL_allocate_sample_handle((HDIGDRIVER)(uintptr_t)&g_dig.pub);
	if (sample != NULL) {
		AIL_set_sample_is_3D(sample, 1);
	}
	return miles_to_3d(sample);
}

void AILCALL AIL_release_3D_sample_handle(RenH3DSAMPLE sample)
{
	AIL_release_sample_handle(miles_to_sample(sample));
}

static S32 miles_upload_3d_sample(RenH3DSAMPLE sample, void const *file_ptr, S32 file_len)
{
	HSAMPLE s = miles_to_sample(sample);
	if (s == NULL || file_ptr == NULL || file_len <= 0) {
		return 0;
	}

	AIL_init_sample(s);
	AIL_set_sample_is_3D(s, 1);
	return AIL_set_named_sample_file(s, NULL, file_ptr, (U32)file_len, -1) ? 1 : 0;
}

S32 AILCALL AIL_set_3D_sample_file_len(RenH3DSAMPLE sample, void const *file_ptr, S32 file_len)
{
	return miles_upload_3d_sample(sample, file_ptr, file_len);
}

S32 AILCALL AIL_set_3D_sample_file(RenH3DSAMPLE sample, void const *file_ptr)
{
	const unsigned char *data = (const unsigned char *)file_ptr;
	U32 len = 0;

	if (data != NULL && memcmp(data, "RIFF", 4) == 0) {
		len = (U32)(data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24)) + 8U;
	}
	if (len == 0) {
		return 0;
	}
	return miles_upload_3d_sample(sample, file_ptr, (S32)len);
}

void AILCALL AIL_start_3D_sample(RenH3DSAMPLE sample)
{
	AIL_start_sample(miles_to_sample(sample));
}

void AILCALL AIL_stop_3D_sample(RenH3DSAMPLE sample)
{
	AIL_stop_sample(miles_to_sample(sample));
}

void AILCALL AIL_resume_3D_sample(RenH3DSAMPLE sample)
{
	AIL_resume_sample(miles_to_sample(sample));
}

void AILCALL AIL_end_3D_sample(RenH3DSAMPLE sample)
{
	AIL_end_sample(miles_to_sample(sample));
}

void AILCALL AIL_set_3D_sample_volume(RenH3DSAMPLE sample, F32 volume)
{
	HSAMPLE s = miles_to_sample(sample);
	F32 current_gain = 1.0f;
	F32 pan = 0.5f;

	if (s == NULL) {
		return;
	}

	AIL_sample_volume_pan(s, &current_gain, &pan);
	AIL_set_sample_volume_pan(s, miles_volume_to_gain(volume), pan);
}

F32 AILCALL AIL_3D_sample_volume(RenH3DSAMPLE sample)
{
	return AIL_sample_volume(miles_to_sample(sample));
}

void AILCALL AIL_set_3D_sample_loop_count(RenH3DSAMPLE sample, S32 count)
{
	AIL_set_sample_loop_count(miles_to_sample(sample), count);
}

S32 AILCALL AIL_3D_sample_loop_count(RenH3DSAMPLE sample)
{
	return AIL_sample_loop_count(miles_to_sample(sample));
}

void AILCALL AIL_set_3D_sample_offset(RenH3DSAMPLE sample, U32 bytes)
{
	AIL_set_sample_position(miles_to_sample(sample), bytes);
}

U32 AILCALL AIL_3D_sample_offset(RenH3DSAMPLE sample)
{
	return AIL_sample_position(miles_to_sample(sample));
}

U32 AILCALL AIL_3D_sample_length(RenH3DSAMPLE sample)
{
	S32 len_ms = 0;
	S32 pos_ms = 0;
	const U32 bytes_per_sec = 44100U * 2U;

	(void)pos_ms;
	AIL_sample_ms_position(miles_to_sample(sample), &len_ms, &pos_ms);
	if (len_ms <= 0) {
		return 0;
	}
	return (U32)(((U32)len_ms * bytes_per_sec) / 1000U);
}

void AILCALL AIL_set_3D_object_user_data(RenH3DSAMPLE sample, U32 index, S32 data)
{
	AIL_set_sample_user_data(miles_to_sample(sample), index, data);
}

S32 AILCALL AIL_3D_object_user_data(RenH3DSAMPLE sample, U32 index)
{
	return AIL_sample_user_data(miles_to_sample(sample), index);
}

S32 AILCALL AIL_3D_sample_playback_rate(RenH3DSAMPLE sample)
{
	return AIL_sample_playback_rate(miles_to_sample(sample));
}

void AILCALL AIL_set_3D_sample_playback_rate(RenH3DSAMPLE sample, S32 rate)
{
	AIL_set_sample_playback_rate(miles_to_sample(sample), rate);
}

void AILCALL AIL_set_3D_position(RenH3DSAMPLE sample, F32 x, F32 y, F32 z)
{
	AIL_set_sample_3D_position(miles_to_sample(sample), x, y, z);
}

void AILCALL AIL_set_3D_orientation(RenH3DSAMPLE sample, F32 x_face, F32 y_face, F32 z_face, F32 x_up, F32 y_up, F32 z_up)
{
	AIL_set_sample_3D_orientation(miles_to_sample(sample), x_face, y_face, z_face, x_up, y_up, z_up);
}

void AILCALL AIL_set_3D_velocity_vector(RenH3DSAMPLE sample, F32 dX_per_ms, F32 dY_per_ms, F32 dZ_per_ms)
{
	AIL_set_sample_3D_velocity_vector(miles_to_sample(sample), dX_per_ms, dY_per_ms, dZ_per_ms);
}

void AILCALL AIL_set_3D_sample_distances(RenH3DSAMPLE sample, F32 max_dist, F32 min_dist)
{
	AIL_set_sample_3D_distances(miles_to_sample(sample), max_dist, min_dist, 1);
}

void AILCALL AIL_set_3D_sample_effects_level(RenH3DSAMPLE sample, F32 level)
{
	F32 dry = 1.0f - level;
	if (dry < 0.0f) {
		dry = 0.0f;
	}
	AIL_set_sample_reverb_levels(miles_to_sample(sample), dry, level);
}

void *AILCALL AIL_3D_open_listener(HPROVIDER provider)
{
	(void)provider;
	return (void *)g_dig.real;
}

void AILCALL AIL_set_room_type(HDIGDRIVER driver, S32 room_type)
{
	miles_bind_real();
	if (g_real_set_room_type != NULL) {
		g_real_set_room_type(miles_real_dig(driver), room_type);
	}
}

S32 AILCALL AIL_room_type(HDIGDRIVER driver)
{
	S32 room_type = ENVIRONMENT_GENERIC;
	miles_bind_real();
	if (g_real_get_room_type != NULL) {
		g_real_get_room_type(miles_real_dig(driver), &room_type);
	}
	return room_type;
}

} /* extern "C" */
