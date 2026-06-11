/*
 * Miles API for Linux (SDL3 audio backend). Declarations match mingw/mss_stub.h.
 */
#ifndef MSS_STUB_LINUX_H
#define MSS_STUB_LINUX_H

#include <windows.h>
#include "../../platform/linux/win32_minimal.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef __stdcall
#define __stdcall
#endif

/* SDL playback tail: device buffer drain after PCM ends (Stop / position logic). */
#define RENEGADE_AUDIO_PLAYBACK_TAIL_MS 350
#ifndef __cdecl
#define __cdecl
#endif
#ifndef LPCTSTR
typedef const char *LPCTSTR;
#endif

#ifndef MSS_TYPE_DEFINED
#define MSS_TYPE_DEFINED
typedef signed int S32;
typedef unsigned int U32;
typedef float F32;
#endif

#ifndef AILCALLBACK
#define AILCALLBACK __cdecl
#endif

#ifndef NO
#define NO 0
#endif

#define AIL_NO_ERROR 0
#define M3D_NOERR 0

#define AIL_LOCK_PROTECTION 90
#define DIG_USE_WAVEOUT 17
#define DP_FILTER 0
#define AIL_3D_2_SPEAKER 0
#define AIL_3D_HEADPHONE 1
#define AIL_3D_SURROUND 2
#define AIL_3D_4_SPEAKER 3
#define ENVIRONMENT_GENERIC 0
#define ENVIRONMENT_PSYCHOTIC 25
#define AIL_FILE_SEEK_BEGIN 0
#define AIL_FILE_SEEK_CURRENT 1
#define AIL_FILE_SEEK_END 2

#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM 1
#endif
#ifndef WAVE_FORMAT_IMA_ADPCM
#define WAVE_FORMAT_IMA_ADPCM 0x0011
#endif

typedef struct waveformat_tag {
	WORD wFormatTag;
	WORD nChannels;
	DWORD nSamplesPerSec;
	DWORD nAvgBytesPerSec;
	WORD nBlockAlign;
} WAVEFORMAT;
typedef WAVEFORMAT *LPWAVEFORMAT;
typedef void *LPHWAVEOUT;

typedef struct {
	WAVEFORMAT wf;
	WORD wBitsPerSample;
} PCMWAVEFORMAT;

struct DIG_DRIVER_TYPE {
	BOOL emulated_ds;
};

typedef DIG_DRIVER_TYPE *HDIGDRIVER;
typedef void *HPROVIDER;
typedef void *HSAMPLE;
typedef void *H3DSAMPLE;
typedef void *H3DPOBJECT;
typedef void *HSTREAM;
typedef void *HTIMER;
typedef S32 HPROENUM;
#define HPROENUM_FIRST ((HPROENUM)-1)

typedef struct {
	S32 format;
	void const *data_ptr;
	U32 data_len;
	U32 rate;
	S32 bits;
	S32 channels;
	U32 samples;
	U32 block_size;
	void const *initial_ptr;
} AILSOUNDINFO;

typedef U32 (AILCALLBACK *AIL_FILE_OPEN_CALLBACK)(char const *filename, U32 *file_handle);
typedef void (AILCALLBACK *AIL_FILE_CLOSE_CALLBACK)(U32 file_handle);
typedef S32 (AILCALLBACK *AIL_FILE_SEEK_CALLBACK)(U32 file_handle, S32 offset, U32 type);
typedef U32 (AILCALLBACK *AIL_FILE_READ_CALLBACK)(U32 file_handle, void *buffer, U32 bytes);

#ifdef __cplusplus
extern "C" {
#endif

void AIL_startup(void);
void AIL_shutdown(void);
void AIL_lock(void);
void AIL_unlock(void);
S32 AIL_set_preference(U32 number, S32 value);
char *AIL_last_error(void);
S32 AIL_waveOutOpen(HDIGDRIVER *driver, LPHWAVEOUT *waveout, unsigned int device_id,
	LPWAVEFORMAT format, unsigned long flags);
void AIL_waveOutClose(HDIGDRIVER driver);
HSAMPLE AIL_allocate_sample_handle(HDIGDRIVER driver);
void AIL_release_sample_handle(HSAMPLE sample);
void AIL_init_sample(HSAMPLE sample);
S32 AIL_set_named_sample_file(HSAMPLE sample, char *filename, void const *file_ptr, S32 file_len, S32 block);
void AIL_start_sample(HSAMPLE sample);
void AIL_stop_sample(HSAMPLE sample);
void AIL_resume_sample(HSAMPLE sample);
void AIL_end_sample(HSAMPLE sample);
S32 AIL_sample_playback_busy(HSAMPLE sample);
void AIL_set_sample_pan(HSAMPLE sample, F32 pan);
F32 AIL_sample_pan(HSAMPLE sample);
void AIL_set_sample_volume(HSAMPLE sample, F32 volume);
F32 AIL_sample_volume(HSAMPLE sample);
void AIL_set_sample_loop_count(HSAMPLE sample, S32 count);
S32 AIL_sample_loop_count(HSAMPLE sample);
void AIL_set_sample_ms_position(HSAMPLE sample, S32 ms);
void AIL_sample_ms_position(HSAMPLE sample, S32 *len, S32 *pos);
void AIL_set_sample_user_data(HSAMPLE sample, U32 index, intptr_t data);
intptr_t AIL_sample_user_data(HSAMPLE sample, U32 index);
S32 AIL_sample_playback_rate(HSAMPLE sample);
void AIL_set_sample_playback_rate(HSAMPLE sample, S32 rate);
void AIL_set_sample_processor(HSAMPLE sample, S32 pipeline_stage, HPROVIDER provider);
void AIL_set_filter_sample_preference(HSAMPLE sample, char const *name, void const *val);
void AIL_set_room_type(HDIGDRIVER driver, S32 room_type);
S32 AIL_room_type(HDIGDRIVER driver);
HSTREAM AIL_open_stream_by_sample(HDIGDRIVER driver, HSAMPLE sample, char const *filename, S32 stream_mem);
void AIL_start_stream(HSTREAM stream);
void AIL_pause_stream(HSTREAM stream, S32 onoff);
void AIL_close_stream(HSTREAM stream);
void AIL_set_stream_pan(HSTREAM stream, F32 pan);
F32 AIL_stream_pan(HSTREAM stream);
void AIL_set_stream_volume(HSTREAM stream, F32 volume);
F32 AIL_stream_volume(HSTREAM stream);
void AIL_set_stream_loop_block(HSTREAM stream, S32 start, S32 end);
void AIL_set_stream_loop_count(HSTREAM stream, S32 count);
S32 AIL_stream_loop_count(HSTREAM stream);
void AIL_set_stream_ms_position(HSTREAM stream, S32 ms);
void AIL_stream_ms_position(HSTREAM stream, S32 *len, S32 *pos);
S32 AIL_stream_playback_rate(HSTREAM stream);
void AIL_set_stream_playback_rate(HSTREAM stream, S32 rate);
S32 AIL_enumerate_3D_providers(HPROENUM *next, HPROVIDER *dest, char **name);
S32 AIL_open_3D_provider(HPROVIDER provider);
void AIL_close_3D_provider(HPROVIDER provider);
void AIL_set_3D_speaker_type(HPROVIDER provider, U32 speaker_type);
H3DSAMPLE AIL_allocate_3D_sample_handle(HPROVIDER provider);
void AIL_release_3D_sample_handle(H3DSAMPLE sample);
S32 AIL_set_3D_sample_file(H3DSAMPLE sample, void const *file_ptr);
S32 AIL_set_3D_sample_file_len(H3DSAMPLE sample, void const *file_ptr, S32 file_len);
void AIL_start_3D_sample(H3DSAMPLE sample);
void AIL_stop_3D_sample(H3DSAMPLE sample);
void AIL_resume_3D_sample(H3DSAMPLE sample);
void AIL_end_3D_sample(H3DSAMPLE sample);
void AIL_set_3D_sample_volume(H3DSAMPLE sample, F32 volume);
F32 AIL_3D_sample_volume(H3DSAMPLE sample);
void AIL_set_3D_sample_loop_count(H3DSAMPLE sample, S32 count);
S32 AIL_3D_sample_loop_count(H3DSAMPLE sample);
void AIL_set_3D_sample_offset(H3DSAMPLE sample, U32 bytes);
U32 AIL_3D_sample_offset(H3DSAMPLE sample);
U32 AIL_3D_sample_length(H3DSAMPLE sample);
void AIL_set_3D_object_user_data(H3DSAMPLE sample, U32 index, intptr_t data);
intptr_t AIL_3D_object_user_data(H3DSAMPLE sample, U32 index);
S32 AIL_3D_sample_playback_rate(H3DSAMPLE sample);
void AIL_set_3D_sample_playback_rate(H3DSAMPLE sample, S32 rate);
void AIL_set_3D_position(H3DSAMPLE sample, F32 x, F32 y, F32 z);
void AIL_set_3D_orientation(H3DSAMPLE sample, F32 x_face, F32 y_face, F32 z_face, F32 x_up, F32 y_up, F32 z_up);
void AIL_set_3D_velocity_vector(H3DSAMPLE sample, F32 dX_per_ms, F32 dY_per_ms, F32 dZ_per_ms);
void AIL_set_3D_sample_distances(H3DSAMPLE sample, F32 max_dist, F32 min_dist);
void AIL_set_3D_sample_effects_level(H3DSAMPLE sample, F32 level);
H3DPOBJECT AIL_3D_open_listener(HPROVIDER provider);
S32 AIL_enumerate_filters(HPROENUM *next, HPROVIDER *dest, char **name);
void AIL_set_file_callbacks(AIL_FILE_OPEN_CALLBACK open_fn, AIL_FILE_CLOSE_CALLBACK close_fn,
	AIL_FILE_SEEK_CALLBACK seek_fn, AIL_FILE_READ_CALLBACK read_fn);
void AIL_stop_timer(HTIMER timer);
void AIL_release_timer_handle(HTIMER timer);
S32 AIL_WAV_info(void const *data, AILSOUNDINFO *info);

#ifdef __cplusplus
}
#endif

#endif
