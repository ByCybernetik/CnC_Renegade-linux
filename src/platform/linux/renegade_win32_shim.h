/*
 * Linux Win32 API declarations (implementations in win32_extra_stubs.cpp).
 */
#ifndef RENEGADE_WINDOWS_H
#define RENEGADE_WINDOWS_H

#include <windows.h>
#include "win32_minimal.h"

#if defined(RENEGADE_WW3D2_BUILD)
#include "ww3d2_win32_extra.h"
#endif

typedef HANDLE HGLOBAL;
typedef HANDLE HFILE;
typedef BOOL *LPBOOL;

#ifndef INFINITE
#define INFINITE 0xFFFFFFFFu
#endif
#ifndef WAIT_OBJECT_0
#define WAIT_OBJECT_0 0
#endif

#define THREAD_PRIORITY_NORMAL 0
#define THREAD_PRIORITY_ABOVE_NORMAL 1
#define THREAD_PRIORITY_BELOW_NORMAL -1

#ifndef FILE_ATTRIBUTE_NORMAL
#define FILE_ATTRIBUTE_NORMAL 0x80
#endif
#ifndef GENERIC_READ
#define GENERIC_READ 0x80000000u
#endif
#ifndef GENERIC_WRITE
#define GENERIC_WRITE 0x40000000u
#endif
#ifndef OPEN_EXISTING
#define OPEN_EXISTING 3
#endif
#ifndef OPEN_ALWAYS
#define OPEN_ALWAYS 4
#endif
#ifndef CREATE_NEW
#define CREATE_NEW 1
#endif
#ifndef CREATE_ALWAYS
#define CREATE_ALWAYS 2
#endif
#ifndef ERROR_ALREADY_EXISTS
#define ERROR_ALREADY_EXISTS 183L
#endif
#ifndef MAX_COMPUTERNAME_LENGTH
#define MAX_COMPUTERNAME_LENGTH 15
#endif
#ifndef FILE_SHARE_READ
#define FILE_SHARE_READ 0x00000001
#endif

#ifdef __cplusplus
extern "C" {
#endif

LONG InterlockedIncrement(LONG volatile *addend);
LONG InterlockedDecrement(LONG volatile *addend);

BOOL AllocConsole(void);
BOOL FreeConsole(void);
HANDLE GetStdHandle(DWORD nStdHandle);
#ifndef STD_INPUT_HANDLE
#define STD_INPUT_HANDLE ((DWORD)-10)
#endif
#ifndef STD_OUTPUT_HANDLE
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#endif
#ifndef STD_ERROR_HANDLE
#define STD_ERROR_HANDLE ((DWORD)-12)
#endif

HWND FindWindowA(LPCSTR className, LPCSTR windowName);
#define FindWindow FindWindowA
BOOL SetForegroundWindow(HWND hwnd);
HWND GetForegroundWindow(void);
HWND GetTopWindow(HWND hwnd);
BOOL ShowWindow(HWND hwnd, int cmdShow);
BOOL GetClientRect(HWND hWnd, RECT *lpRect);
BOOL GetDiskFreeSpaceExA(LPCSTR path, PULARGE_INTEGER freeBytes, PULARGE_INTEGER totalBytes,
	PULARGE_INTEGER totalFreeBytes);
#define GetDiskFreeSpaceEx GetDiskFreeSpaceExA
UINT GetSystemDirectoryA(LPSTR buffer, UINT size);
#define GetSystemDirectory GetSystemDirectoryA
BOOL GetDiskFreeSpaceA(LPCSTR path, LPDWORD sectorsPerCluster, LPDWORD bytesPerSector,
	LPDWORD freeClusters, LPDWORD totalClusters);
#define GetDiskFreeSpace GetDiskFreeSpaceA
#ifndef SW_RESTORE
#define SW_RESTORE 9
#endif

void ExitProcess(UINT uExitCode);
BOOL GetComputerNameA(LPSTR lpBuffer, LPDWORD nSize);
#define GetComputerName GetComputerNameA
BOOL GetUserNameA(LPSTR lpBuffer, LPDWORD pcbBuffer);
#define GetUserName GetUserNameA
#ifndef STILL_ACTIVE
#define STILL_ACTIVE 259
#endif
#ifndef PROCESS_TERMINATE
#define PROCESS_TERMINATE 0x0001
#endif
HANDLE OpenProcess(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId);
BOOL TerminateProcess(HANDLE hProcess, UINT uExitCode);
DWORD GetWindowThreadProcessId(HWND hWnd, LPDWORD lpdwProcessId);

DWORD GetCurrentThreadId(void);
DWORD GetCurrentProcessId(void);
HANDLE CreateEventA(void *attr, BOOL manual, BOOL initial, LPCSTR name);
#define CreateEvent CreateEventA
HANDLE CreateMutexA(void *attr, BOOL initial_owner, LPCSTR name);
#define CreateMutex CreateMutexA
HANDLE OpenMutexA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName);
#define OpenMutex OpenMutexA
#ifndef MUTEX_ALL_ACCESS
#define MUTEX_ALL_ACCESS 0x001F0001
#endif
BOOL ReleaseMutex(HANDLE mutex);
DWORD GetProcessVersion(DWORD processId);
BOOL SetEvent(HANDLE h);
DWORD WaitForSingleObject(HANDLE h, DWORD ms);
#ifndef WAIT_TIMEOUT
#define WAIT_TIMEOUT 0x00000102L
#endif
BOOL CloseHandle(HANDLE h);
BOOL ReadFile(HANDLE h, LPVOID buf, DWORD bytes, LPDWORD read, LPVOID overlapped);
BOOL WriteFile(HANDLE h, LPCVOID buf, DWORD bytes, LPDWORD written, LPVOID overlapped);
DWORD GetFileSize(HANDLE h, LPDWORD high);
#ifndef INVALID_FILE_SIZE
#define INVALID_FILE_SIZE ((DWORD)0xFFFFFFFFu)
#endif
HFILE CreateFileA(LPCSTR name, DWORD access, DWORD share, LPVOID sec, DWORD disp, DWORD flags, HANDLE templ);
#define CreateFile CreateFileA
BOOL SetThreadPriority(HANDLE thread, int priority);
BOOL TerminateThread(HANDLE thread, DWORD code);
HANDLE LoadLibraryA(LPCSTR name);
#define LoadLibrary LoadLibraryA
void *GetProcAddress(HMODULE mod, LPCSTR name);
BOOL FreeLibrary(HMODULE mod);
DWORD GetLastError(void);
void SetLastError(DWORD err);
HMODULE GetModuleHandleA(LPCSTR name);
#define GetModuleHandle GetModuleHandleA
DWORD GetModuleFileNameA(HMODULE module, LPSTR filename, DWORD size);
#define GetModuleFileName GetModuleFileNameA
DWORD GetFileAttributesA(LPCSTR path);
int ShowCursor(BOOL bShow);
BOOL GetCursorPos(LPPOINT lpPoint);
BOOL SetCursorPos(int x, int y);
BOOL GetExitCodeProcess(HANDLE proc, DWORD *code);
int ToAscii(UINT vk, UINT scan, PBYTE keystate, LPWORD buffer, UINT flags);

void OutputDebugStringA(LPCSTR str);
#define OutputDebugString OutputDebugStringA

#define VER_PLATFORM_WIN32s 0
#define VER_PLATFORM_WIN32_WINDOWS 1
#define VER_PLATFORM_WIN32_NT 2

typedef struct _REN_MEMORYSTATUS {
	DWORD dwLength;
	DWORD dwMemoryLoad;
	DWORD dwTotalPhys;
	DWORD dwAvailPhys;
	DWORD dwTotalPageFile;
	DWORD dwAvailPageFile;
	DWORD dwTotalVirtual;
	DWORD dwAvailVirtual;
} REN_MEMORYSTATUS, *LPREN_MEMORYSTATUS;

#ifndef MEMORYSTATUS
#define MEMORYSTATUS REN_MEMORYSTATUS
#define LPMEMORYSTATUS LPREN_MEMORYSTATUS
#endif

typedef struct _OSVERSIONINFOA {
	DWORD dwOSVersionInfoSize;
	DWORD dwMajorVersion;
	DWORD dwMinorVersion;
	DWORD dwBuildNumber;
	DWORD dwPlatformId;
	CHAR szCSDVersion[128];
} OSVERSIONINFOA;
typedef OSVERSIONINFOA OSVERSIONINFO;

typedef struct _TIME_ZONE_INFORMATION {
	LONG Bias;
	WORD StandardDate[8];
	WORD StandardName[32];
	WORD DaylightDate[8];
	WORD DaylightName[32];
} TIME_ZONE_INFORMATION, *LPTIME_ZONE_INFORMATION;

BOOL GlobalMemoryStatus(LPMEMORYSTATUS status);
BOOL GetVersionExA(OSVERSIONINFOA *info);
#define GetVersionEx GetVersionExA
DWORD GetTimeZoneInformation(TIME_ZONE_INFORMATION *tz);

BOOL QueryPerformanceFrequency(LARGE_INTEGER *freq);
BOOL QueryPerformanceCounter(LARGE_INTEGER *counter);
HANDLE GetCurrentProcess(void);
HANDLE GetCurrentThread(void);
DWORD GetPriorityClass(HANDLE process);
BOOL SetPriorityClass(HANDLE process, DWORD priority);
int GetThreadPriority(HANDLE thread);

#define REALTIME_PRIORITY_CLASS 0x100
#define THREAD_PRIORITY_TIME_CRITICAL 15

#ifndef _MAX_DRIVE
#define _MAX_DRIVE 3
#endif
#ifndef _MAX_DIR
#define _MAX_DIR 256
#endif
#ifndef _MAX_FNAME
#define _MAX_FNAME 256
#endif
#ifndef _MAX_EXT
#define _MAX_EXT 256
#endif

void _splitpath(const char *path, char *drive, char *dir, char *fname, char *ext);
void _makepath(char *path, const char *drive, const char *dir, const char *fname, const char *ext);

#define DRIVE_UNKNOWN 0
#define DRIVE_CDROM 5
UINT GetDriveTypeA(LPCSTR root);
#define GetDriveType GetDriveTypeA
DWORD GetLogicalDriveStringsA(DWORD nBufferLength, LPSTR lpBuffer);
#define GetLogicalDriveStrings GetLogicalDriveStringsA
#define SearchPath SearchPathA
UINT GetWindowsDirectoryA(LPSTR lpBuffer, UINT uSize);
#define GetWindowsDirectory GetWindowsDirectoryA
UINT GetTempFileNameA(LPCSTR lpPathName, LPCSTR lpPrefixString, UINT uUnique, LPSTR lpTempFileName);
#define GetTempFileName GetTempFileNameA
DWORD WaitForInputIdle(HANDLE hProcess, DWORD dwMilliseconds);
BOOL GetVolumeInformationA(LPCSTR root, LPSTR vol, DWORD volSize, LPDWORD serial,
	LPDWORD maxComp, LPDWORD flags, LPSTR fs, DWORD fsSize);
#define GetVolumeInformation GetVolumeInformationA
BOOL DeleteFileA(LPCSTR path);
BOOL MoveFileA(LPCSTR from, LPCSTR to);
BOOL CreateDirectoryA(LPCSTR path, LPVOID attr);
#define CreateDirectory CreateDirectoryA
#define DeleteFile DeleteFileA
#define MoveFile MoveFileA
#define GetFileAttributes GetFileAttributesA

#define FILE_ATTRIBUTE_READONLY 0x1
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)

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

HANDLE FindFirstFileA(LPCSTR path, WIN32_FIND_DATAA *data);
BOOL FindNextFileA(HANDLE find, WIN32_FIND_DATAA *data);
BOOL FindClose(HANDLE find);
#define FindFirstFile FindFirstFileA
#define FindNextFile FindNextFileA

typedef void *HRSRC;

HRSRC FindResourceA(HMODULE module, LPCSTR name, LPCSTR type);
HGLOBAL LoadResource(HMODULE module, HRSRC res);
void *LockResource(HGLOBAL res);
DWORD SizeofResource(HMODULE module, HRSRC res);
#define FindResource FindResourceA

typedef struct _IMAGE_FILE_HEADER {
	WORD Machine;
	WORD NumberOfSections;
	DWORD TimeDateStamp;
	DWORD PointerToSymbolTable;
	DWORD NumberOfSymbols;
	WORD SizeOfOptionalHeader;
	WORD Characteristics;
} IMAGE_FILE_HEADER, *PIMAGE_FILE_HEADER;

#define CP_ACP 0

DWORD GetTickCount(void);
#define GetCurrentTime GetTickCount
int WideCharToMultiByte(UINT codepage, DWORD flags, LPCWSTR src, int srclen, LPSTR dst, int dstlen,
	LPCSTR def, LPBOOL used);
int MultiByteToWideChar(UINT codepage, DWORD flags, LPCSTR src, int srclen, LPWSTR dst, int dstlen);
DWORD GetCurrentDirectoryA(DWORD buflen, LPSTR out);
#define GetCurrentDirectory GetCurrentDirectoryA
BOOL SetCurrentDirectoryA(LPCSTR path);
#define SetCurrentDirectory SetCurrentDirectoryA
DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, LONG *lpDistanceToMoveHigh, DWORD dwMoveMethod);
#ifndef FILE_END
#define FILE_END 2
#endif
#ifndef FILE_BEGIN
#define FILE_BEGIN 0
#endif
#ifndef FILE_CURRENT
#define FILE_CURRENT 1
#endif
#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER ((DWORD)-1)
#endif

#ifdef __cplusplus
}
#endif

#include "winreg.h"

#endif /* RENEGADE_WINDOWS_H */
