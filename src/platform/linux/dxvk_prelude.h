/* Force-included on all Linux TUs before dxvk windows_base.h */
#ifndef RENEGADE_DXVK_PRELUDE_H
#define RENEGADE_DXVK_PRELUDE_H

#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#if defined(RENEGADE_LINUX)
#ifndef __int64
typedef long long __int64;
#endif
#ifndef _int64
#define _int64 __int64
#endif
#ifndef _stdcall
#define _stdcall __stdcall
#endif

#include <wchar.h>
#include <strings.h>
#ifndef wcsicmp
#define wcsicmp wcscasecmp
#endif
#ifndef wcsnicmp
#define wcsnicmp wcsncasecmp
#endif
#ifndef _wcsnicmp
#define _wcsnicmp wcsnicmp
#endif
#include <wchar.h>
#include <wctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <alloca.h>
#ifndef _alloca
#define _alloca alloca
#endif
#ifndef itoa
static inline char *renegade_itoa(int value, char *buf, int radix)
{
	if (radix == 10) {
		sprintf(buf, "%d", value);
	} else if (radix == 16) {
		sprintf(buf, "%x", value);
	} else {
		sprintf(buf, "%d", value);
	}
	return buf;
}
#define itoa renegade_itoa
#endif
#ifndef _itoa
#define _itoa itoa
#endif
#ifndef _itow
static inline wchar_t *renegade_itow(int value, wchar_t *buffer, int radix)
{
	if (radix == 16) {
		swprintf(buffer, 32, L"%x", value);
	} else {
		swprintf(buffer, 32, L"%d", value);
	}
	return buffer;
}
#define _itow renegade_itow
#endif
#ifndef _wtol
static inline long renegade_wtol(const wchar_t *s)
{
	return wcstol(s, NULL, 10);
}
#define _wtol renegade_wtol
#endif

#include <strings.h>
#include <ctype.h>
#ifndef strnicmp
#define strnicmp strncasecmp
#endif
#ifndef _strnicmp
#define _strnicmp strnicmp
#endif
#ifndef stricmp
#define stricmp strcasecmp
#endif
#ifndef _stricmp
#define _stricmp stricmp
#endif
#ifndef _strupr
static inline char *renegade_strupr(char *s)
{
	if (s) {
		for (char *p = s; *p; ++p) {
			*p = (char)toupper((unsigned char)*p);
		}
	}
	return s;
}
#define _strupr renegade_strupr
#endif

#ifndef __declspec
#define __declspec(x)
#endif

#ifndef WINAPI
#define WINAPI
#endif
#ifndef APIENTRY
#define APIENTRY WINAPI
#endif
#ifndef CALLBACK
#define CALLBACK WINAPI
#endif

#ifndef DLL_PROCESS_ATTACH
#define DLL_PROCESS_ATTACH 1
#endif
#ifndef DLL_PROCESS_DETACH
#define DLL_PROCESS_DETACH 0
#endif
#ifndef DLL_THREAD_ATTACH
#define DLL_THREAD_ATTACH 2
#endif
#ifndef DLL_THREAD_DETACH
#define DLL_THREAD_DETACH 3
#endif
#endif

#endif
