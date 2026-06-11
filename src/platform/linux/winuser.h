#ifndef RENEGADE_WINUSER_H
#define RENEGADE_WINUSER_H

#include <windows.h>
#include "winuser_extra.h"

#ifndef MAKELONG
#define MAKELONG(a, b) ((LONG)(((WORD)(a)) | ((DWORD)((WORD)(b))) << 16))
#endif

#ifndef LOCALE_USER_DEFAULT
#define LOCALE_USER_DEFAULT 0x0400
#endif
#ifndef LOCALE_SYSTEM_DEFAULT
#define LOCALE_SYSTEM_DEFAULT 0x0800
#endif
#ifndef TIME_FORCE24HOURFORMAT
#define TIME_FORCE24HOURFORMAT 0x00000008
#endif

#ifndef MB_OK
#define MB_OK 0x00000000
#endif
#ifndef MB_ICONERROR
#define MB_ICONERROR 0x00000010
#endif
#ifndef MB_ICONEXCLAMATION
#define MB_ICONEXCLAMATION 0x00000030
#endif
#ifndef MB_SETFOREGROUND
#define MB_SETFOREGROUND 0x00010000
#endif
#ifndef SW_MINIMIZE
#define SW_MINIMIZE 6
#endif
#ifndef SW_SHOW
#define SW_SHOW 5
#endif
#ifndef NORM_IGNORECASE
#define NORM_IGNORECASE 0x00000001
#endif

#define LVS_NOCOLUMNHEADER 0x4000

#define CSTR_LESS_THAN 1
#define CSTR_EQUAL 2
#define CSTR_GREATER_THAN 3

typedef struct _SYSTEMTIME {
	WORD wYear;
	WORD wMonth;
	WORD wDayOfWeek;
	WORD wDay;
	WORD wHour;
	WORD wMinute;
	WORD wSecond;
	WORD wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;

#ifndef RENEGADE_FILETIME_DEFINED
#define RENEGADE_FILETIME_DEFINED
typedef struct _FILETIME {
	DWORD dwLowDateTime;
	DWORD dwHighDateTime;
} FILETIME;
#endif

#ifdef __cplusplus
extern "C" {
#endif

int CompareStringW(LCID locale, DWORD flags, LPCWSTR s1, int c1, LPCWSTR s2, int c2);
BOOL IsDBCSLeadByte(BYTE test);
BOOL SystemParametersInfoA(UINT action, UINT param, PVOID pvParam, UINT winIni);
#define SystemParametersInfo SystemParametersInfoA
int AddFontResourceA(LPCSTR name);
#define AddFontResource AddFontResourceA
BOOL RemoveFontResourceA(LPCSTR name);
#define RemoveFontResource RemoveFontResourceA
void GetSystemTime(LPSYSTEMTIME lpSystemTime);

typedef FILETIME *LPFILETIME;
BOOL FileTimeToSystemTime(const FILETIME *lpFileTime, LPSYSTEMTIME lpSystemTime);
BOOL FileTimeToLocalFileTime(const FILETIME *lpFileTime, LPFILETIME lpLocalFileTime);
LONG CompareFileTime(const FILETIME *ft1, const FILETIME *ft2);
BOOL GetProcessTimes(HANDLE hProcess, LPFILETIME lpCreationTime, LPFILETIME lpExitTime,
	LPFILETIME lpKernelTime, LPFILETIME lpUserTime);
int GetDateFormatA(LCID Locale, DWORD dwFlags, const SYSTEMTIME *lpDate, LPCSTR lpFormat,
	LPSTR lpDateStr, int cchDate);
#define GetDateFormat GetDateFormatA
int GetTimeFormatA(LCID Locale, DWORD dwFlags, const SYSTEMTIME *lpTime, LPCSTR lpFormat,
	LPSTR lpTimeStr, int cchTime);
#define GetTimeFormat GetTimeFormatA
int MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType);
#define MessageBox MessageBoxA

#ifndef RENEGADE_HACCEL_DEFINED
#define RENEGADE_HACCEL_DEFINED
typedef HANDLE HACCEL;
#endif
HACCEL LoadAcceleratorsA(HMODULE hInstance, LPCSTR lpTableName);
#define LoadAccelerators LoadAcceleratorsA

#ifdef __cplusplus
}
#endif

#endif
