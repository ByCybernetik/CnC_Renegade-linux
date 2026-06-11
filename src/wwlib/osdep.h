/*
 * Westwood UNIX portability (missing from migrated tree; used with -D_UNIX).
 */
#ifndef WWLIB_OSDEP_H
#define WWLIB_OSDEP_H

#if defined(_UNIX) || defined(RENEGADE_LINUX)

#include <alloca.h>
#include <ctype.h>
#include <time.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>

#ifndef _alloca
#define _alloca alloca
#endif
#ifndef _snprintf
#define _snprintf snprintf
#endif
#ifndef _vsnprintf
#define _vsnprintf vsnprintf
#endif
#if !defined(RENEGADE_LINUX) && !defined(_vsnwprintf)
#define _vsnwprintf vswprintf
#endif
#if !defined(RENEGADE_LINUX) && !defined(_wcsicmp)
#define _wcsicmp wcscasecmp
#endif
#include <wchar.h>

#ifndef stricmp
#define stricmp strcasecmp
#endif
#ifndef strnicmp
#define strnicmp strncasecmp
#endif
#ifndef strcmpi
#define strcmpi strcasecmp
#endif
#ifndef _stricmp
#define _stricmp strcasecmp
#endif
#ifndef _strnicmp
#define _strnicmp strncasecmp
#endif
#ifndef _strdup
#define _strdup strdup
#endif
#ifndef lstrcmpi
#define lstrcmpi strcasecmp
#endif
#ifndef lstrlen
#define lstrlen strlen
#endif
#ifndef lstrcat
#define lstrcat strcat
#endif
#ifndef lstrcpy
#define lstrcpy strcpy
#endif
static inline void lstrcpyn(char *dst, const char *src, int n)
{
	if (!dst || n <= 0) {
		return;
	}
	strncpy(dst, src ? src : "", (size_t)n);
	dst[n - 1] = '\0';
}

static inline char *strupr(char *s)
{
	if (s) {
		for (char *p = s; *p; ++p) {
			*p = (char)toupper((unsigned char)*p);
		}
	}
	return s;
}

#define strlwr _strlwr
static inline char *_strlwr(char *s)
{
	if (s) {
		for (char *p = s; *p; ++p) {
			*p = (char)tolower((unsigned char)*p);
		}
	}
	return s;
}

static inline unsigned long _lrotl(unsigned long value, int shift)
{
	shift &= 31;
	return (value << shift) | (value >> (32 - shift));
}

static inline int _wtoi(const wchar_t *s)
{
	return s ? (int)wcstol(s, NULL, 10) : 0;
}

#if !defined(RENEGADE_LINUX)
static inline wchar_t *_wcsupr(wchar_t *s)
{
	if (s) {
		for (wchar_t *p = s; *p; ++p) {
			if (*p >= L'a' && *p <= L'z') {
				*p = (wchar_t)(*p - (L'a' - L'A'));
			}
		}
	}
	return s;
}
#endif /* !RENEGADE_LINUX */

#endif /* _UNIX || RENEGADE_LINUX */

#endif /* WWLIB_OSDEP_H */
