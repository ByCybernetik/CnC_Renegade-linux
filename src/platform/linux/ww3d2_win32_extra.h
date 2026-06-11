#ifndef WW3D2_WIN32_EXTRA_H
#define WW3D2_WIN32_EXTRA_H
#include <windows.h>

#define FILE_ATTRIBUTE_DIRECTORY 0x10
#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
#endif

#ifndef RENEGADE_FILETIME_DEFINED
#define RENEGADE_FILETIME_DEFINED
typedef struct _FILETIME {
	DWORD dwLowDateTime;
	DWORD dwHighDateTime;
} FILETIME;
#endif

#ifndef RENEGADE_WIN32_FIND_DATA_DEFINED
#define RENEGADE_WIN32_FIND_DATA_DEFINED
typedef struct _WIN32_FIND_DATAA {
	DWORD dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD nFileSizeHigh;
	DWORD nFileSizeLow;
	DWORD dwReserved0;
	DWORD dwReserved1;
	CHAR cFileName[MAX_PATH];
	CHAR cAlternateFileName[14];
} WIN32_FIND_DATAA;
typedef WIN32_FIND_DATAA WIN32_FIND_DATA;
#endif

#ifndef MB_OK
#define MB_OK 0x00000000u
#endif

#ifdef __cplusplus
extern "C" {
#endif

DWORD GetCurrentDirectoryA(DWORD buflen, LPSTR out);
#define GetCurrentDirectory GetCurrentDirectoryA
BOOL SetCurrentDirectoryA(LPCSTR path);
#define SetCurrentDirectory SetCurrentDirectoryA
DWORD GetFileAttributesA(LPCSTR path);
#define GetFileAttributes GetFileAttributesA
HANDLE FindFirstFileA(LPCSTR path, WIN32_FIND_DATAA *data);
BOOL FindNextFileA(HANDLE find, WIN32_FIND_DATAA *data);
BOOL FindClose(HANDLE find);
#define FindFirstFile FindFirstFileA
#define FindNextFile FindNextFileA
int MessageBoxA(HWND hwnd, LPCSTR text, LPCSTR caption, UINT type);
#define MessageBox MessageBoxA

#ifdef __cplusplus
}
#endif

#endif
