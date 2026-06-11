/*
** Strong Miles API overrides linked with --whole-archive so they replace mss32.lib
** thunks for lock/startup/WAV probe. Avoids Wine crashes from mss32.dll entry points
** during SoundBufferClass::Load_From_File (AIL_lock, AIL_WAV_info).
*/

#include "mss_stub.h"
#include "Utils.h"
#include "miles_win32_util.h"

#include <string.h>
#include <windows.h>

namespace {

typedef void (__stdcall *MilesStartupFn)(void);
typedef void (__stdcall *MilesShutdownFn)(void);
typedef void (__stdcall *MilesSetRedistFn)(char const *dir);

static MilesStartupFn g_real_startup = NULL;
static MilesShutdownFn g_real_shutdown = NULL;
static bool g_redist_set = false;

static HMODULE miles_module(void)
{
	HMODULE mod = GetModuleHandleA("mss32.dll");
	if (mod == NULL) {
		mod = LoadLibraryA("mss32.dll");
	}
	return mod;
}

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

	HMODULE mod = miles_module();
	if (mod != NULL) {
		MilesSetRedistFn set_redist = (MilesSetRedistFn)miles_get_proc(
			mod, "AIL_set_redist_directory", "_AIL_set_redist_directory@4");
		if (set_redist != NULL) {
			set_redist(path);
			g_redist_set = true;
		}
	}
}

static void miles_bind_startup(void)
{
	HMODULE mod;

	if (g_real_startup != NULL) {
		return;
	}

	mod = miles_module();
	if (mod == NULL) {
		return;
	}

	g_real_startup = (MilesStartupFn)miles_get_proc(mod, "AIL_startup", "_AIL_startup@0");
	g_real_shutdown = (MilesShutdownFn)miles_get_proc(mod, "AIL_shutdown", "_AIL_shutdown@0");
}

static unsigned long miles_read_le32(unsigned char const *p)
{
	return (unsigned long)p[0] | ((unsigned long)p[1] << 8) |
		((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

static unsigned short miles_read_le16(unsigned char const *p)
{
	return (unsigned short)((unsigned short)p[0] | ((unsigned short)p[1] << 8));
}

} /* namespace */

extern "C" {

void AILCALL AIL_lock(void)
{
	EnterCriticalSection(&MMSLockClass::_MSSLockCriticalSection);
}

void AILCALL AIL_unlock(void)
{
	LeaveCriticalSection(&MMSLockClass::_MSSLockCriticalSection);
}

void AILCALL AIL_lock_mutex(void)
{
	AIL_lock();
}

void AILCALL AIL_unlock_mutex(void)
{
	AIL_unlock();
}

void AILCALL AIL_startup(void)
{
	miles_set_redist_directory();
	miles_bind_startup();
	if (g_real_startup != NULL) {
		g_real_startup();
	}
}

void AILCALL AIL_shutdown(void)
{
	miles_bind_startup();
	if (g_real_shutdown != NULL) {
		g_real_shutdown();
	}
}

S32 AILCALL AIL_WAV_info(void const *data, AILSOUNDINFO *info)
{
	unsigned char const *wav;
	unsigned char const *cursor;
	unsigned char const *end;
	unsigned long riff_size;
	unsigned long chunk_size;
	S32 format_tag;
	S32 channels;
	S32 bits;
	U32 rate;

	if (data == NULL || info == NULL) {
		return 0;
	}

	memset(info, 0, sizeof(*info));
	wav = (unsigned char const *)data;
	if (memcmp(wav, "RIFF", 4) != 0 || memcmp(wav + 8, "WAVE", 4) != 0) {
		return 0;
	}

	riff_size = miles_read_le32(wav + 4);
	end = wav + 8 + riff_size;
	if (end <= wav + 12) {
		end = wav + 65536;
	}

	cursor = wav + 12;
	format_tag = 0;
	channels = 0;
	bits = 0;
	rate = 0;

	while (cursor + 8 <= end) {
		chunk_size = miles_read_le32(cursor + 4);
		if (memcmp(cursor, "fmt ", 4) == 0 && chunk_size >= 16 && cursor + 8 + 16 <= end) {
			unsigned char const *fmt = cursor + 8;
			format_tag = (S32)miles_read_le16(fmt);
			channels = (S32)miles_read_le16(fmt + 2);
			rate = miles_read_le32(fmt + 4);
			bits = (S32)miles_read_le16(fmt + 14);
			break;
		}
		cursor += 8 + chunk_size + (chunk_size & 1U);
	}

	if (rate <= 0 || channels <= 0) {
		return 0;
	}
	if (bits <= 0) {
		bits = 16;
	}

	info->format = format_tag;
	info->rate = rate;
	info->channels = channels;
	info->bits = bits;
	info->data_ptr = data;
	info->data_len = riff_size + 8U;
	info->samples = 0;
	info->block_size = 0;
	info->initial_ptr = data;
	return 1;
}

} /* extern "C" */
