/*
 * MinGW link stubs for optional DLLs / MSVC-only pieces.
 */
#define BANDTEST_EXPORTS
#include "BandTest/BandTest.h"
#include "soundrobj.h"
#include <stdio.h>
#include <stdarg.h>

unsigned long Detect_Bandwidth(
	unsigned long,
	unsigned long,
	int,
	int &failure_code,
	unsigned long &downstream,
	unsigned long,
	tBandtestSettingsStruct *,
	char *)
{
	failure_code = BANDTEST_UNKNOWN_ERROR;
	downstream = 0;
	return 0;
}

SoundRenderObjLoaderClass _SoundRenderObjLoader;

PrototypeClass *SoundRenderObjLoaderClass::Load_W3D(ChunkLoadClass &)
{
	return NULL;
}

extern "C" int __conio_common_vcprintf(
	unsigned __int64,
	FILE *,
	const char *,
	void *,
	va_list)
{
	return 0;
}
