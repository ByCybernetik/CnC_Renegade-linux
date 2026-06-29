/*
 * Minimal Miles Sound System (MSS) declarations for linking WWAudio without
 * the proprietary Miles SDK. Implementations are no-ops in mss_stubs.cpp.
 *
 * On Linux, this header defines all needed Windows types so that the
 * VC6 audio code compiles without any <windows.h> dependency.
 */
#ifndef MSS_STUB_H
#define MSS_STUB_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ---- Portable Windows type stand-ins (Linux only) ---- */
/* Use typedefs instead of defines to avoid clashing with DXVK windows_base.h. */
#if defined(RENEGADE_LINUX)
typedef int BOOL;
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
typedef void* HANDLE;
typedef const char* LPCSTR;
typedef const char* LPCTSTR;
typedef const wchar_t* LPCWSTR;
typedef void VOID;
typedef void* PVOID;
typedef void* LPVOID;
#endif /* RENEGADE_LINUX */

/* Win32 constants (Linux stubs) */
#if defined(RENEGADE_LINUX)
#ifndef INFINITE
#define INFINITE 0xFFFFFFFF
#endif
#ifndef WAIT_OBJECT_0
#define WAIT_OBJECT_0 0
#endif
#ifndef WAIT_TIMEOUT
#define WAIT_TIMEOUT 0x00000102
#endif
#ifndef WAIT_FAILED
#define WAIT_FAILED 0xFFFFFFFF
#endif

/* TIMEGETTIME: MSS time-get macro, maps to timeGetTime stub */
#ifndef TIMEGETTIME
#define TIMEGETTIME() timeGetTime()
#endif
#endif /* RENEGADE_LINUX */

/* WAVEOUT stubs (never called on Linux) */
#if defined(RENEGADE_LINUX)
typedef void* HWAVEOUT;
typedef HWAVEOUT* LPHWAVEOUT;

/* Minimal WAVEFORMATEX for audio device config (never used on Linux) */
#ifndef _WORD_DEFINED
#define _WORD_DEFINED
typedef uint16_t WORD;
#endif
#ifndef _DWORD_DEFINED
#define _DWORD_DEFINED
typedef uint32_t DWORD;
#endif
#ifndef _LONG_DEFINED
#define _LONG_DEFINED
typedef int32_t LONG;
#endif
#ifndef _UINT_DEFINED
#define _UINT_DEFINED
typedef unsigned int UINT;
#endif

typedef struct {
	WORD wFormatTag;
	WORD nChannels;
	DWORD nSamplesPerSec;
	DWORD nAvgBytesPerSec;
	WORD nBlockAlign;
} WAVEFORMAT;

typedef struct {
	WORD wFormatTag;
	WORD nChannels;
	DWORD nSamplesPerSec;
	DWORD nAvgBytesPerSec;
	WORD nBlockAlign;
	WORD wBitsPerSample;
	WORD cbSize;
} WAVEFORMATEX;

typedef WAVEFORMATEX* LPWAVEFORMATEX;
typedef WAVEFORMAT* LPWAVEFORMAT;

typedef struct {
	WAVEFORMAT wf;
	WORD wBitsPerSample;
} PCMWAVEFORMAT;

/* Threading stubs */
#ifndef _CRITICAL_SECTION_DEFINED
#define _CRITICAL_SECTION_DEFINED
typedef struct { int dummy; } CRITICAL_SECTION, *LPCRITICAL_SECTION;
#endif

/* COM / HRESULT stubs (never called on Linux) */
typedef int32_t HRESULT;

/* Win32 API stubs for threading & events */
#ifdef __cplusplus
extern "C" {
#endif
HANDLE CreateEventA(void* attr, int manual_reset, int initial_state, const char* name);
HANDLE CreateEventW(void* attr, int manual_reset, int initial_state, const wchar_t* name);
BOOL SetEvent(HANDLE hEvent);
BOOL ResetEvent(HANDLE hEvent);
DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
BOOL CloseHandle(HANDLE hObject);
DWORD timeGetTime(void);
#ifdef __cplusplus
}
#endif

/* CreateEvent maps to CreateEventA (ANSI) */
#ifndef CreateEvent
#define CreateEvent CreateEventA
#endif

#endif /* RENEGADE_LINUX */

#if defined(RENEGADE_LINUX) && !defined(MSS_TYPE_DEFINED)
#define MSS_TYPE_DEFINED
typedef int32_t S32;
typedef uint32_t U32;
typedef float F32;
#endif

#if defined(RENEGADE_LINUX)

#define AILCALLBACK
#define AILCALL

#else
#include <windows.h>
#include <mmsystem.h>

#ifndef AILCALLBACK
#define AILCALLBACK __cdecl
#endif

#ifndef AILCALL
#define AILCALL __stdcall
#endif
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

struct DIG_DRIVER_TYPE
{
	BOOL emulated_ds;
};

typedef DIG_DRIVER_TYPE * HDIGDRIVER;
typedef void * HPROVIDER;
typedef void * HSAMPLE;
typedef void * H3DSAMPLE;
typedef void * H3DPOBJECT;
typedef void * HSTREAM;
typedef void * HTIMER;

typedef S32 HPROENUM;
#define HPROENUM_FIRST ((HPROENUM)-1)

typedef struct
{
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

typedef U32 (AILCALLBACK *AIL_FILE_OPEN_CALLBACK)(char const *filename, intptr_t *file_handle);
typedef void (AILCALLBACK *AIL_FILE_CLOSE_CALLBACK)(intptr_t file_handle);
typedef S32 (AILCALLBACK *AIL_FILE_SEEK_CALLBACK)(intptr_t file_handle, S32 offset, U32 type);
typedef U32 (AILCALLBACK *AIL_FILE_READ_CALLBACK)(intptr_t file_handle, void *buffer, U32 bytes);

#ifdef __cplusplus
extern "C" {
#endif

void AILCALL AIL_startup(void);
void AILCALL AIL_shutdown(void);
void AILCALL AIL_lock(void);
void AILCALL AIL_unlock(void);

S32 AILCALL AIL_set_preference(U32 number, S32 value);
char *AILCALL AIL_last_error(void);

S32 AILCALL AIL_waveOutOpen(HDIGDRIVER *driver, LPHWAVEOUT *waveout, unsigned int device_id, LPWAVEFORMAT format, unsigned long flags = 0);
void AILCALL AIL_waveOutClose(HDIGDRIVER driver);

HSAMPLE AILCALL AIL_allocate_sample_handle(HDIGDRIVER driver);
void AILCALL AIL_release_sample_handle(HSAMPLE sample);
void AILCALL AIL_init_sample(HSAMPLE sample);
S32 AILCALL AIL_set_named_sample_file(HSAMPLE sample, char *filename, void const *file_ptr, S32 file_len, S32 block);
void AILCALL AIL_start_sample(HSAMPLE sample);
void AILCALL AIL_stop_sample(HSAMPLE sample);
void AILCALL AIL_resume_sample(HSAMPLE sample);
void AILCALL AIL_end_sample(HSAMPLE sample);
S32 AILCALL AIL_sample_playback_busy(HSAMPLE sample);
void AILCALL AIL_set_sample_pan(HSAMPLE sample, F32 pan);
F32 AILCALL AIL_sample_pan(HSAMPLE sample);
void AILCALL AIL_set_sample_volume(HSAMPLE sample, F32 volume);
F32 AILCALL AIL_sample_volume(HSAMPLE sample);
void AILCALL AIL_set_sample_loop_count(HSAMPLE sample, S32 count);
S32 AILCALL AIL_sample_loop_count(HSAMPLE sample);
void AILCALL AIL_set_sample_ms_position(HSAMPLE sample, S32 ms);
void AILCALL AIL_sample_ms_position(HSAMPLE sample, S32 *len, S32 *pos);
void AILCALL AIL_set_sample_user_data(HSAMPLE sample, U32 index, intptr_t data);
intptr_t AILCALL AIL_sample_user_data(HSAMPLE sample, U32 index);
S32 AILCALL AIL_sample_playback_rate(HSAMPLE sample);
void AILCALL AIL_set_sample_playback_rate(HSAMPLE sample, S32 rate);
void AILCALL AIL_set_sample_processor(HSAMPLE sample, S32 pipeline_stage, HPROVIDER provider);
void AILCALL AIL_set_filter_sample_preference(HSAMPLE sample, char const *name, void const *val);
void AILCALL AIL_set_room_type(HDIGDRIVER driver, S32 room_type);
S32 AILCALL AIL_room_type(HDIGDRIVER driver);

HSTREAM AILCALL AIL_open_stream_by_sample(HDIGDRIVER driver, HSAMPLE sample, char const *filename, S32 stream_mem);
void AILCALL AIL_start_stream(HSTREAM stream);
void AILCALL AIL_pause_stream(HSTREAM stream, S32 onoff);
void AILCALL AIL_close_stream(HSTREAM stream);
void AILCALL AIL_set_stream_pan(HSTREAM stream, F32 pan);
F32 AILCALL AIL_stream_pan(HSTREAM stream);
void AILCALL AIL_set_stream_volume(HSTREAM stream, F32 volume);
F32 AILCALL AIL_stream_volume(HSTREAM stream);
void AILCALL AIL_set_stream_loop_block(HSTREAM stream, S32 start, S32 end);
void AILCALL AIL_set_stream_loop_count(HSTREAM stream, S32 count);
S32 AILCALL AIL_stream_loop_count(HSTREAM stream);
void AILCALL AIL_set_stream_ms_position(HSTREAM stream, S32 ms);
void AILCALL AIL_stream_ms_position(HSTREAM stream, S32 *len, S32 *pos);
S32 AILCALL AIL_stream_playback_rate(HSTREAM stream);
void AILCALL AIL_set_stream_playback_rate(HSTREAM stream, S32 rate);

S32 AILCALL AIL_enumerate_3D_providers(HPROENUM *next, HPROVIDER *dest, char **name);
S32 AILCALL AIL_open_3D_provider(HPROVIDER provider);
void AILCALL AIL_close_3D_provider(HPROVIDER provider);
void AILCALL AIL_set_3D_speaker_type(HPROVIDER provider, U32 speaker_type);

H3DSAMPLE AILCALL AIL_allocate_3D_sample_handle(HPROVIDER provider);
void AILCALL AIL_release_3D_sample_handle(H3DSAMPLE sample);
S32 AILCALL AIL_set_3D_sample_file(H3DSAMPLE sample, void const *file_ptr);
/* Renegade extension: explicit size (raw buffer from SoundBufferClass). */
S32 AILCALL AIL_set_3D_sample_file_len(H3DSAMPLE sample, void const *file_ptr, S32 file_len);
void AILCALL AIL_start_3D_sample(H3DSAMPLE sample);
void AILCALL AIL_stop_3D_sample(H3DSAMPLE sample);
void AILCALL AIL_resume_3D_sample(H3DSAMPLE sample);
void AILCALL AIL_end_3D_sample(H3DSAMPLE sample);
void AILCALL AIL_set_3D_sample_volume(H3DSAMPLE sample, F32 volume);
F32 AILCALL AIL_3D_sample_volume(H3DSAMPLE sample);
void AILCALL AIL_set_3D_sample_loop_count(H3DSAMPLE sample, S32 count);
S32 AILCALL AIL_3D_sample_loop_count(H3DSAMPLE sample);
void AILCALL AIL_set_3D_sample_offset(H3DSAMPLE sample, U32 bytes);
U32 AILCALL AIL_3D_sample_offset(H3DSAMPLE sample);
U32 AILCALL AIL_3D_sample_length(H3DSAMPLE sample);
void AILCALL AIL_set_3D_object_user_data(H3DSAMPLE sample, U32 index, intptr_t data);
intptr_t AILCALL AIL_3D_object_user_data(H3DSAMPLE sample, U32 index);
S32 AILCALL AIL_3D_sample_playback_rate(H3DSAMPLE sample);
void AILCALL AIL_set_3D_sample_playback_rate(H3DSAMPLE sample, S32 rate);

void AILCALL AIL_set_3D_position(H3DSAMPLE sample, F32 x, F32 y, F32 z);
void AILCALL AIL_set_3D_orientation(H3DSAMPLE sample, F32 x_face, F32 y_face, F32 z_face, F32 x_up, F32 y_up, F32 z_up);
void AILCALL AIL_set_3D_velocity_vector(H3DSAMPLE sample, F32 dX_per_ms, F32 dY_per_ms, F32 dZ_per_ms);
void AILCALL AIL_set_3D_sample_distances(H3DSAMPLE sample, F32 max_dist, F32 min_dist);
void AILCALL AIL_set_3D_sample_effects_level(H3DSAMPLE sample, F32 level);

H3DPOBJECT AILCALL AIL_3D_open_listener(HPROVIDER provider);

S32 AILCALL AIL_enumerate_filters(HPROENUM *next, HPROVIDER *dest, char **name);
void AILCALL AIL_set_file_callbacks(AIL_FILE_OPEN_CALLBACK open_fn, AIL_FILE_CLOSE_CALLBACK close_fn,
	AIL_FILE_SEEK_CALLBACK seek_fn, AIL_FILE_READ_CALLBACK read_fn);

void AILCALL AIL_stop_timer(HTIMER timer);
void AILCALL AIL_release_timer_handle(HTIMER timer);

S32 AILCALL AIL_WAV_info(void const *data, AILSOUNDINFO *info);

#ifdef __cplusplus
}
#endif

#endif /* MSS_STUB_H */
