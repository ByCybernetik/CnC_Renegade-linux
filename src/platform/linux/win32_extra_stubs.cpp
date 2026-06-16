#include "renegade_win32_shim.h"
#include "linux_sync.h"
#include "objbase.h"
#include "winerror.h"
#include "../sdl3_host.h"
#include <SDL3/SDL.h>
#include "conio.h"
#include "shellapi.h"
#include "wincon.h"
#include <stdarg.h>
#include <poll.h>
#include <sys/statvfs.h>
#include "commctrl.h"
#include "winuser_extra.h"
#include "mmsystem.h"
#include "process.h"
#include <cstdio>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <wchar.h>
#include "../../wwlib/ww_wcstring.h"
#include <sched.h>
#include <unistd.h>
#include <dlfcn.h>
#include <ctype.h>
#include <limits.h>
#include <fnmatch.h>
#include <errno.h>
#include <strings.h>
#include "linux_sync.h"

static DWORD g_last_error = 0;

static void renegade_path_normalize(char *out, size_t out_cap, const char *in)
{
	size_t j = 0;
	if (out == NULL || out_cap == 0) {
		return;
	}
	if (in == NULL) {
		out[0] = '\0';
		return;
	}
	for (size_t i = 0; in[i] != '\0' && j + 1 < out_cap; ++i) {
		out[j++] = (in[i] == '\\') ? '/' : in[i];
	}
	out[j] = '\0';
}

static bool renegade_resolve_path_ci(char *resolved, size_t cap, const char *in)
{
	char norm[1024];
	char component[256];
	const char *p;

	if (resolved == NULL || cap == 0 || in == NULL) {
		return false;
	}

	renegade_path_normalize(norm, sizeof(norm), in);
	if (norm[0] == '\0') {
		return false;
	}

	resolved[0] = '\0';
	p = norm;
	if (norm[0] == '/') {
		strncpy(resolved, "/", cap - 1);
		resolved[cap - 1] = '\0';
		p++;
	}

	while (*p != '\0') {
		size_t i = 0;

		while (*p == '/') {
			p++;
		}
		if (*p == '\0') {
			break;
		}

		while (*p != '\0' && *p != '/' && i + 1 < sizeof(component)) {
			component[i++] = *p++;
		}
		component[i] = '\0';
		if (i == 0) {
			continue;
		}

		char search_dir[1024];
		if (resolved[0] == '\0') {
			strncpy(search_dir, ".", sizeof(search_dir) - 1);
		} else {
			strncpy(search_dir, resolved, sizeof(search_dir) - 1);
		}
		search_dir[sizeof(search_dir) - 1] = '\0';

		DIR *dir = opendir(search_dir);
		if (dir == NULL) {
			return false;
		}

		bool found = false;
		struct dirent *ent;
		while ((ent = readdir(dir)) != NULL) {
			if (strcasecmp(ent->d_name, component) != 0) {
				continue;
			}
			found = true;
			size_t len = strlen(resolved);
			if (len > 0 && resolved[len - 1] != '/' && len + 1 < cap) {
				strncat(resolved, "/", cap - len - 1);
			}
			strncat(resolved, ent->d_name, cap - strlen(resolved) - 1);
			break;
		}
		closedir(dir);
		if (!found) {
			return false;
		}
	}

	return resolved[0] != '\0';
}

static bool renegade_stat_ci(const char *path, struct stat *st)
{
	char normalized[1024];
	char resolved[1024];

	if (path == NULL || st == NULL) {
		return false;
	}

	renegade_path_normalize(normalized, sizeof(normalized), path);
	if (stat(normalized, st) == 0) {
		return true;
	}
	if (renegade_resolve_path_ci(resolved, sizeof(resolved), path)) {
		return stat(resolved, st) == 0;
	}
	return false;
}

static bool renegade_path_is_valid_utf8(const char *path)
{
	mbstate_t state;
	const char *p;

	if (path == NULL) {
		return true;
	}
	memset(&state, 0, sizeof(state));
	p = path;
	while (*p != '\0') {
		wchar_t ch;
		const size_t n = mbrtowc(&ch, p, (size_t)MB_CUR_MAX, &state);
		if (n == (size_t)-1 || n == (size_t)-2 || n == 0) {
			return false;
		}
		p += n;
	}
	return true;
}

static bool renegade_path_looks_like_wide_leak(const char *path)
{
	size_t len;
	size_t i;

	if (path == NULL || path[0] == '\0') {
		return false;
	}
	len = strlen(path);
	if (len < 2) {
		return false;
	}
	if (renegade_path_is_valid_utf8(path)) {
		return false;
	}
	for (i = 0; i + 1 < len; i += 2) {
		const unsigned char lo = (unsigned char)path[i];
		const unsigned char hi = (unsigned char)path[i + 1];
		if (hi == 0x04u && lo >= 0x10u) {
			return true;
		}
		if (hi == 0x00u && lo < 0x80u) {
			return true;
		}
	}
	return true;
}

static BOOL renegade_mkdir_p(const char *path)
{
	char norm[1024];
	char parent[1024];
	char *slash;
	struct stat st;

	if (path == NULL || path[0] == '\0') {
		SetLastError(ERROR_PATH_NOT_FOUND);
		return FALSE;
	}

	renegade_path_normalize(norm, sizeof(norm), path);
	if (stat(norm, &st) == 0) {
		if (S_ISDIR(st.st_mode)) {
			return TRUE;
		}
		SetLastError(ERROR_ALREADY_EXISTS);
		return FALSE;
	}

	strncpy(parent, norm, sizeof(parent) - 1);
	parent[sizeof(parent) - 1] = '\0';
	slash = strrchr(parent, '/');
	if (slash != NULL && slash != parent) {
		*slash = '\0';
		if (!renegade_mkdir_p(parent)) {
			return FALSE;
		}
	}

	if (mkdir(norm, 0755) == 0) {
		return TRUE;
	}
	if (errno == EEXIST) {
		if (stat(norm, &st) == 0 && S_ISDIR(st.st_mode)) {
			return TRUE;
		}
		SetLastError(ERROR_ALREADY_EXISTS);
		return FALSE;
	}
	SetLastError(ERROR_PATH_NOT_FOUND);
	return FALSE;
}

DWORD GetCurrentThreadId(void) { return (DWORD)pthread_self(); }

DWORD GetCurrentProcessId(void) { return (DWORD)getpid(); }

DWORD GetLastError(void) { return g_last_error; }

void SetLastError(DWORD err) { g_last_error = err; }

HANDLE CreateEventA(void *, BOOL manual_reset, BOOL initial_state, LPCSTR)
{
	return renegade_sync_create_event(manual_reset, initial_state);
}

BOOL SetEvent(HANDLE h) { return renegade_sync_set_event(h); }

BOOL ResetEvent(HANDLE h) { return renegade_sync_reset_event(h); }

DWORD WaitForSingleObject(HANDLE h, DWORD ms)
{
	if (renegade_sync_is_typed_handle(h)) {
		return renegade_sync_wait_for_single_object(h, ms);
	}
	(void)h;
	(void)ms;
	return WAIT_OBJECT_0;
}

struct FindHandle {
	DIR *dir;
	char dirpath[MAX_PATH];
	char wild[MAX_PATH];
};

static void normalize_path_slashes(const char *in, char *out, size_t out_cap)
{
	size_t j = 0;
	for (size_t i = 0; in[i] != '\0' && j + 1 < out_cap; ++i) {
		out[j++] = (in[i] == '\\') ? '/' : in[i];
	}
	out[j] = '\0';
}

static void unix_time_to_filetime(time_t sec, FILETIME *ft)
{
	const unsigned long long epoch = 116444736000000000ULL;
	const unsigned long long ticks = (unsigned long long)sec * 10000000ULL + epoch;
	ft->dwLowDateTime = (DWORD)(ticks & 0xFFFFFFFFULL);
	ft->dwHighDateTime = (DWORD)(ticks >> 32);
}

static void parse_find_path(LPCSTR path, char *dirpath, char *wild)
{
	strncpy(dirpath, ".", MAX_PATH - 1);
	dirpath[MAX_PATH - 1] = '\0';
	strncpy(wild, "*", MAX_PATH - 1);
	wild[MAX_PATH - 1] = '\0';
	if (!path) {
		return;
	}

	char buf[MAX_PATH];
	strncpy(buf, path, MAX_PATH - 1);
	buf[MAX_PATH - 1] = '\0';

	char *last_sep = NULL;
	for (char *p = buf; *p != '\0'; ++p) {
		if (*p == '/' || *p == '\\') {
			last_sep = p;
		}
	}

	if (last_sep != NULL) {
		*last_sep = '\0';
		normalize_path_slashes(buf, dirpath, MAX_PATH);
		strncpy(wild, last_sep + 1, MAX_PATH - 1);
		wild[MAX_PATH - 1] = '\0';
	} else {
		strncpy(wild, buf, MAX_PATH - 1);
		wild[MAX_PATH - 1] = '\0';
	}
}

static void ascii_tolower_copy(const char *src, char *dst, size_t cap)
{
	size_t i = 0;
	for (; src[i] != '\0' && i + 1 < cap; ++i) {
		unsigned char c = (unsigned char)src[i];
		if (c >= 'A' && c <= 'Z') {
			c += ('a' - 'A');
		}
		dst[i] = (char)c;
	}
	dst[i] = '\0';
}

static int ascii_strieq(const char *a, const char *b)
{
	while (*a && *b) {
		unsigned char ca = (unsigned char)*a++;
		unsigned char cb = (unsigned char)*b++;
		if (ca >= 'A' && ca <= 'Z') {
			ca += ('a' - 'A');
		}
		if (cb >= 'A' && cb <= 'Z') {
			cb += ('a' - 'A');
		}
		if (ca != cb) {
			return 0;
		}
	}
	return *a == *b;
}

static int append_path_component(char *base, size_t base_cap, const char *component)
{
	if (!component || component[0] == '\0') {
		return 0;
	}
	const char *parent = (base[0] != '\0') ? base : ".";
	DIR *parent_dir = opendir(parent);
	if (parent_dir == NULL) {
		return 0;
	}

	struct dirent *ent;
	int found = 0;
	while ((ent = readdir(parent_dir)) != NULL) {
		if (ent->d_name[0] == '.' && (ent->d_name[1] == '\0' ||
				(ent->d_name[1] == '.' && ent->d_name[2] == '\0'))) {
			continue;
		}
		if (!ascii_strieq(ent->d_name, component)) {
			continue;
		}
		if (base[0] == '\0') {
			snprintf(base, base_cap, "%s", ent->d_name);
		} else {
			snprintf(base, base_cap, "%s/%s", base, ent->d_name);
		}
		found = 1;
		break;
	}
	closedir(parent_dir);
	return found;
}

static DIR *opendir_resolving_case(const char *dirpath, char *resolved, size_t resolved_cap)
{
	char norm[MAX_PATH];
	normalize_path_slashes(dirpath, norm, sizeof(norm));

	DIR *dir = opendir(norm);
	if (dir != NULL) {
		strncpy(resolved, norm, resolved_cap - 1);
		resolved[resolved_cap - 1] = '\0';
		return dir;
	}

	if (norm[0] == '\0' || strcmp(norm, ".") == 0) {
		strncpy(resolved, ".", resolved_cap - 1);
		resolved[resolved_cap - 1] = '\0';
		return opendir(".");
	}

	char built[MAX_PATH];
	built[0] = '\0';

	char walk[MAX_PATH];
	strncpy(walk, norm, sizeof(walk) - 1);
	walk[sizeof(walk) - 1] = '\0';

	char *cursor = walk;
	if (walk[0] == '/') {
		built[0] = '/';
		built[1] = '\0';
		cursor = walk + 1;
	}

	while (cursor != NULL && *cursor != '\0') {
		char *next = strchr(cursor, '/');
		if (next != NULL) {
			*next = '\0';
		}
		if (!append_path_component(built, sizeof(built), cursor)) {
			return NULL;
		}
		if (next != NULL) {
			cursor = next + 1;
		} else {
			break;
		}
	}

	dir = opendir(built);
	if (dir != NULL) {
		strncpy(resolved, built, resolved_cap - 1);
		resolved[resolved_cap - 1] = '\0';
	}
	return dir;
}

static int name_matches(const char *name, const char *wild)
{
	if (!wild || wild[0] == '\0' || strcmp(wild, "*") == 0) {
		return 1;
	}
	// Windows FindFirstFile is case-insensitive; Linux fnmatch is not.
	char lname[MAX_PATH];
	char lwild[MAX_PATH];
	ascii_tolower_copy(name, lname, sizeof(lname));
	ascii_tolower_copy(wild, lwild, sizeof(lwild));
	return fnmatch(lwild, lname, 0) == 0;
}

static BOOL fill_find_entry(FindHandle *fh, WIN32_FIND_DATAA *data)
{
	for (;;) {
		struct dirent *ent = readdir(fh->dir);
		if (!ent) {
			return FALSE;
		}
		if (ent->d_name[0] == '.' && (ent->d_name[1] == '\0' ||
				(ent->d_name[1] == '.' && ent->d_name[2] == '\0'))) {
			continue;
		}
		if (!name_matches(ent->d_name, fh->wild)) {
			continue;
		}
		memset(data, 0, sizeof(*data));
		strncpy(data->cFileName, ent->d_name, MAX_PATH - 1);
		data->cFileName[MAX_PATH - 1] = '\0';
		char full[MAX_PATH * 2];
		snprintf(full, sizeof(full), "%s/%s", fh->dirpath, ent->d_name);
		struct stat st;
		if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
			data->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		} else {
			data->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
			unix_time_to_filetime(st.st_mtime, &data->ftLastWriteTime);
		}
		return TRUE;
	}
}

HANDLE FindFirstFileA(LPCSTR path, WIN32_FIND_DATAA *data)
{
	if (!path || !data) {
		return INVALID_HANDLE_VALUE;
	}
	FindHandle *fh = (FindHandle *)calloc(1, sizeof(FindHandle));
	if (!fh) {
		return INVALID_HANDLE_VALUE;
	}
	parse_find_path(path, fh->dirpath, fh->wild);
	fh->dir = opendir_resolving_case(fh->dirpath, fh->dirpath, MAX_PATH);
	if (!fh->dir) {
		free(fh);
		return INVALID_HANDLE_VALUE;
	}
	if (!fill_find_entry(fh, data)) {
		closedir(fh->dir);
		free(fh);
		return INVALID_HANDLE_VALUE;
	}
	return (HANDLE)fh;
}

BOOL FindNextFileA(HANDLE find, WIN32_FIND_DATAA *data)
{
	FindHandle *fh = (FindHandle *)find;
	if (!fh || !fh->dir || !data) {
		return FALSE;
	}
	return fill_find_entry(fh, data);
}

BOOL FindClose(HANDLE find)
{
	FindHandle *fh = (FindHandle *)find;
	if (!fh) {
		return FALSE;
	}
	if (fh->dir) {
		closedir(fh->dir);
	}
	free(fh);
	return TRUE;
}

BOOL CloseHandle(HANDLE h)
{
	if (renegade_sync_is_typed_handle(h)) {
		return renegade_sync_close_handle(h);
	}
	if (!h || h == INVALID_HANDLE_VALUE) {
		return FALSE;
	}
	return fclose((FILE *)h) == 0;
}

BOOL ReadFile(HANDLE h, LPVOID buf, DWORD bytes, LPDWORD read, LPVOID)
{
	FILE *f = (FILE *)h;
	if (!f || !buf) {
		return FALSE;
	}
	size_t got = fread(buf, 1, bytes, f);
	if (read) {
		*read = (DWORD)got;
	}
	return got > 0 || feof(f);
}

BOOL WriteFile(HANDLE h, LPCVOID buf, DWORD bytes, LPDWORD written, LPVOID)
{
	FILE *f = (FILE *)h;
	if (!f || !buf) {
		return FALSE;
	}
	size_t put = fwrite(buf, 1, bytes, f);
	if (written) {
		*written = (DWORD)put;
	}
	return put == bytes;
}

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, LONG *lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
	FILE *f = (FILE *)hFile;
	if (!f) {
		return INVALID_SET_FILE_POINTER;
	}
	int whence = SEEK_SET;
	if (dwMoveMethod == FILE_CURRENT) {
		whence = SEEK_CUR;
	} else if (dwMoveMethod == FILE_END) {
		whence = SEEK_END;
	}
	if (fseek(f, lDistanceToMove, whence) != 0) {
		return INVALID_SET_FILE_POINTER;
	}
	long pos = ftell(f);
	if (lpDistanceToMoveHigh) {
		*lpDistanceToMoveHigh = 0;
	}
	return pos >= 0 ? (DWORD)pos : INVALID_SET_FILE_POINTER;
}

DWORD GetFileAttributesA(LPCSTR path)
{
	if (!path || !path[0]) {
		return INVALID_FILE_ATTRIBUTES;
	}
	struct stat st;
	if (!renegade_stat_ci(path, &st)) {
		return INVALID_FILE_ATTRIBUTES;
	}
	DWORD attr = FILE_ATTRIBUTE_NORMAL;
	if (S_ISDIR(st.st_mode)) {
		attr |= FILE_ATTRIBUTE_DIRECTORY;
	}
	return attr;
}

HANDLE CreateFileA(LPCSTR name, DWORD access, DWORD, LPVOID, DWORD disp, DWORD, HANDLE)
{
	char normalized[1024];
	const char *path;

	if (!name) {
		return INVALID_HANDLE_VALUE;
	}

	renegade_path_normalize(normalized, sizeof(normalized), name);
	path = normalized;

	if (renegade_path_looks_like_wide_leak(path)) {
		SetLastError(ERROR_PATH_NOT_FOUND);
		return INVALID_HANDLE_VALUE;
	}

	if (access & GENERIC_WRITE) {
		char *slash = strrchr(normalized, '/');
		if (slash != NULL && slash != normalized) {
			*slash = '\0';
			renegade_mkdir_p(normalized);
			*slash = '/';
		}
	}

	const char *mode = "rb";
	if (access & GENERIC_WRITE) {
		if (disp == CREATE_NEW) {
			FILE *existing = fopen(path, "rb");
			if (existing) {
				fclose(existing);
				SetLastError(ERROR_ALREADY_EXISTS);
				return INVALID_HANDLE_VALUE;
			}
			mode = "wb";
		} else if (disp == CREATE_ALWAYS) {
			mode = "wb";
		} else {
			mode = "r+b";
		}
	}
	FILE *f = fopen(path, mode);
	return f ? (HANDLE)f : INVALID_HANDLE_VALUE;
}

static DWORD platform_tick_ms(void)
{
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
		return 0;
	}
	return (DWORD)(ts.tv_sec * 1000ull + ts.tv_nsec / 1000000ull);
}

DWORD GetCurrentDirectoryA(DWORD buflen, LPSTR out)
{
	if (!out || buflen == 0) {
		return 0;
	}
	return getcwd(out, buflen) ? (DWORD)strlen(out) : 0;
}

BOOL SetCurrentDirectoryA(LPCSTR path)
{
	return path && chdir(path) == 0;
}

BOOL InitCommonControlsEx(LPINITCOMMONCONTROLSEX) { return TRUE; }

HKL GetKeyboardLayout(DWORD) { return (HKL)0; }

int GetKeyboardLayoutList(int, HKL *) { return 0; }

int CompareStringW(LCID, DWORD, LPCWSTR s1, int c1, LPCWSTR s2, int c2)
{
	(void)s1;
	(void)c1;
	(void)s2;
	(void)c2;
	return 1;
}

BOOL IsDBCSLeadByte(BYTE) { return FALSE; }

BOOL SystemParametersInfoA(UINT, UINT, PVOID pvParam, UINT)
{
	if (pvParam) {
		*(UINT *)pvParam = 3;
	}
	return TRUE;
}

int AddFontResourceA(LPCSTR) { return 0; }

BOOL RemoveFontResourceA(LPCSTR) { return TRUE; }

BOOL FileTimeToSystemTime(const FILETIME *ft, LPSYSTEMTIME st)
{
	if (!ft || !st) {
		return FALSE;
	}
	const unsigned long long ticks = ((unsigned long long)ft->dwHighDateTime << 32)
		| ft->dwLowDateTime;
	const unsigned long long epoch = 116444736000000000ULL;
	if (ticks < epoch) {
		return FALSE;
	}
	const time_t sec = (time_t)((ticks - epoch) / 10000000ULL);
	struct tm tm_buf;
#ifdef _POSIX_THREAD_SAFE_FUNCTIONS
	gmtime_r(&sec, &tm_buf);
#else
	struct tm *p = gmtime(&sec);
	if (!p) {
		return FALSE;
	}
	tm_buf = *p;
#endif
	st->wYear = (WORD)(1900 + tm_buf.tm_year);
	st->wMonth = (WORD)(1 + tm_buf.tm_mon);
	st->wDayOfWeek = (WORD)tm_buf.tm_wday;
	st->wDay = (WORD)tm_buf.tm_mday;
	st->wHour = (WORD)tm_buf.tm_hour;
	st->wMinute = (WORD)tm_buf.tm_min;
	st->wSecond = (WORD)tm_buf.tm_sec;
	st->wMilliseconds = (WORD)((ticks / 10000ULL) % 1000);
	return TRUE;
}

LONG CompareFileTime(const FILETIME *a, const FILETIME *b)
{
	if (!a || !b) {
		return 0;
	}
	unsigned long long ta = ((unsigned long long)a->dwHighDateTime << 32) | a->dwLowDateTime;
	unsigned long long tb = ((unsigned long long)b->dwHighDateTime << 32) | b->dwLowDateTime;
	if (ta < tb) {
		return -1;
	}
	if (ta > tb) {
		return 1;
	}
	return 0;
}

BOOL FileTimeToLocalFileTime(const FILETIME *in, LPFILETIME out)
{
	if (!in || !out) {
		return FALSE;
	}
	*out = *in;
	return TRUE;
}

BOOL GetProcessTimes(HANDLE, LPFILETIME creation, LPFILETIME exit, LPFILETIME kernel, LPFILETIME user)
{
	FILETIME ft = {0, 0};
	if (creation) {
		*creation = ft;
	}
	if (exit) {
		*exit = ft;
	}
	if (kernel) {
		*kernel = ft;
	}
	if (user) {
		*user = ft;
	}
	return TRUE;
}

static void SystemTimeToTm(const SYSTEMTIME *st, struct tm *out)
{
	out->tm_year = st->wYear - 1900;
	out->tm_mon = st->wMonth - 1;
	out->tm_mday = st->wDay;
	out->tm_hour = st->wHour;
	out->tm_min = st->wMinute;
	out->tm_sec = st->wSecond;
	out->tm_isdst = -1;
}

int GetDateFormatA(LCID, DWORD, const SYSTEMTIME *lpDate, LPCSTR, LPSTR lpDateStr, int cchDate)
{
	if (!lpDate || !lpDateStr || cchDate <= 0) {
		return 0;
	}
	struct tm tm_buf;
	SystemTimeToTm(lpDate, &tm_buf);
	return (int)strftime(lpDateStr, (size_t)cchDate, "%m/%d/%Y", &tm_buf);
}

int GetTimeFormatA(LCID, DWORD, const SYSTEMTIME *lpTime, LPCSTR, LPSTR lpTimeStr, int cchTime)
{
	if (!lpTime || !lpTimeStr || cchTime <= 0) {
		return 0;
	}
	struct tm tm_buf;
	SystemTimeToTm(lpTime, &tm_buf);
	return (int)strftime(lpTimeStr, (size_t)cchTime, "%H:%M:%S", &tm_buf);
}

HINSTANCE FindExecutableA(LPCSTR file, LPCSTR, LPSTR result)
{
	if (!file || !result) {
		return (HINSTANCE)32;
	}
	strncpy(result, file, MAX_PATH - 1);
	result[MAX_PATH - 1] = '\0';
	return (HINSTANCE)32;
}

HINSTANCE ShellExecuteA(HWND, LPCSTR operation, LPCSTR file, LPCSTR params, LPCSTR, INT)
{
	if (!file || !file[0]) {
		return (HINSTANCE)0;
	}
	char cmd[2048];
	if (operation && strcmp(operation, "open") == 0) {
		snprintf(cmd, sizeof(cmd), "xdg-open '%s'", file);
	} else if (params && params[0]) {
		snprintf(cmd, sizeof(cmd), "%s %s", file, params);
	} else {
		snprintf(cmd, sizeof(cmd), "%s", file);
	}
	int rc = system(cmd);
	return (rc == 0) ? (HINSTANCE)42 : (HINSTANCE)0;
}

int _kbhit(void)
{
	struct pollfd pfd;
	pfd.fd = 0;
	pfd.events = POLLIN;
	pfd.revents = 0;
	return poll(&pfd, 1, 0) > 0 ? 1 : 0;
}

int _getch(void) { return getchar(); }

int _getche(void)
{
	int c = getchar();
	if (c != EOF) {
		putchar(c);
		fflush(stdout);
	}
	return c;
}

BOOL AllocConsole(void) { return TRUE; }

BOOL FreeConsole(void) { return TRUE; }

HANDLE GetStdHandle(DWORD which)
{
	(void)which;
	return (HANDLE)1;
}

HWND FindWindowA(LPCSTR, LPCSTR) { return NULL; }

BOOL SetForegroundWindow(HWND) { return TRUE; }

HWND GetForegroundWindow(void) { return NULL; }

HWND GetTopWindow(HWND hwnd) { (void)hwnd; return NULL; }

BOOL ShowWindow(HWND hwnd, int cmdShow)
{
	return Platform_Show_Window(hwnd, cmdShow);
}
BOOL GetClientRect(HWND hwnd, RECT *rect)
{
	if (!rect) {
		return FALSE;
	}
	SDL_Window *window = (SDL_Window *)hwnd;
	if (window == NULL) {
		rect->left = rect->top = 0;
		rect->right = 640;
		rect->bottom = 480;
		return TRUE;
	}
	int w = 0;
	int h = 0;
	if (!SDL_GetWindowSize(window, &w, &h)) {
		rect->left = rect->top = 0;
		rect->right = 640;
		rect->bottom = 480;
		return TRUE;
	}
	rect->left = rect->top = 0;
	rect->right = w;
	rect->bottom = h;
	return TRUE;
}

BOOL GetDiskFreeSpaceExA(LPCSTR path, PULARGE_INTEGER free, PULARGE_INTEGER total, PULARGE_INTEGER totalFree)
{
	struct statvfs st;
	const char *root = path;

	if (root == NULL || root[0] == '\0') {
		root = ".";
	}

	if (statvfs(root, &st) == 0) {
		unsigned long long free_bytes = (unsigned long long)st.f_bavail * st.f_frsize;
		if (free) {
			free->QuadPart = free_bytes;
		}
		if (total) {
			total->QuadPart = (unsigned long long)st.f_blocks * st.f_frsize;
		}
		if (totalFree) {
			totalFree->QuadPart = (unsigned long long)st.f_bfree * st.f_frsize;
		}
		return TRUE;
	}
	return FALSE;
}
#define GetDiskFreeSpaceEx GetDiskFreeSpaceExA

UINT GetSystemDirectoryA(LPSTR buffer, UINT size)
{
	const char *dir = "/usr/lib";
	if (!buffer || size == 0) {
		return 0;
	}
	strncpy(buffer, dir, size - 1);
	buffer[size - 1] = '\0';
	return (UINT)strlen(buffer);
}

BOOL GetDiskFreeSpaceA(LPCSTR path, LPDWORD spc, LPDWORD bps, LPDWORD freec, LPDWORD totalc)
{
	ULARGE_INTEGER free_bytes, total_bytes, total_free;
	if (!GetDiskFreeSpaceExA(path, &free_bytes, &total_bytes, &total_free)) {
		return FALSE;
	}
	if (bps) {
		*bps = 512;
	}
	if (spc) {
		*spc = 8;
	}
	if (freec) {
		*freec = (DWORD)(free_bytes.QuadPart / (512 * 8));
	}
	if (totalc) {
		*totalc = (DWORD)(total_bytes.QuadPart / (512 * 8));
	}
	return TRUE;
}

BOOL SetConsoleScreenBufferSize(HANDLE, COORD) { return TRUE; }

BOOL FillConsoleOutputAttribute(HANDLE, WORD, DWORD, COORD, LPDWORD written)
{
	if (written) {
		*written = 0;
	}
	return TRUE;
}

BOOL SetConsoleTextAttribute(HANDLE, WORD) { return TRUE; }

BOOL SetConsoleTitleA(LPCSTR title)
{
	if (title) {
		fprintf(stderr, "[console] %s\n", title);
	}
	return TRUE;
}

BOOL GetConsoleScreenBufferInfo(HANDLE, CONSOLE_SCREEN_BUFFER_INFO *info)
{
	if (info) {
		memset(info, 0, sizeof(*info));
		info->dwSize.X = 80;
		info->dwSize.Y = 25;
	}
	return TRUE;
}

BOOL SetConsoleCursorPosition(HANDLE, COORD) { return TRUE; }

BOOL FillConsoleOutputCharacterA(HANDLE, CHAR, DWORD, COORD, LPDWORD written)
{
	if (written) {
		*written = 0;
	}
	return TRUE;
}

void GetLocalTime(LPSYSTEMTIME st)
{
	if (!st) {
		return;
	}
	time_t now = time(NULL);
	struct tm tm_buf;
#ifdef _POSIX_THREAD_SAFE_FUNCTIONS
	localtime_r(&now, &tm_buf);
#else
	struct tm *p = localtime(&now);
	if (p) {
		tm_buf = *p;
	}
#endif
	st->wYear = (WORD)(1900 + tm_buf.tm_year);
	st->wMonth = (WORD)(1 + tm_buf.tm_mon);
	st->wDayOfWeek = (WORD)tm_buf.tm_wday;
	st->wDay = (WORD)(tm_buf.tm_mday);
	st->wHour = (WORD)tm_buf.tm_hour;
	st->wMinute = (WORD)tm_buf.tm_min;
	st->wSecond = (WORD)tm_buf.tm_sec;
	st->wMilliseconds = 0;
}

BOOL SystemTimeToFileTime(const SYSTEMTIME *st, LPFILETIME ft)
{
	if (!st || !ft) {
		return FALSE;
	}
	struct tm tm_buf;
	SystemTimeToTm(st, &tm_buf);
	time_t sec = mktime(&tm_buf);
	if (sec == (time_t)-1) {
		return FALSE;
	}
	unsigned long long ticks = (unsigned long long)sec * 10000000ULL + 116444736000000000ULL;
	ft->dwLowDateTime = (DWORD)(ticks & 0xffffffffu);
	ft->dwHighDateTime = (DWORD)(ticks >> 32);
	return TRUE;
}

int cprintf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int n = vfprintf(stdout, fmt, ap);
	va_end(ap);
	fflush(stdout);
	return n;
}

void GetSystemTime(LPSYSTEMTIME st)
{
	if (!st) {
		return;
	}
	time_t now = time(NULL);
	struct tm tm_buf;
#ifdef _POSIX_THREAD_SAFE_FUNCTIONS
	gmtime_r(&now, &tm_buf);
#else
	struct tm *p = gmtime(&now);
	if (p) {
		tm_buf = *p;
	}
#endif
	st->wYear = (WORD)(1900 + tm_buf.tm_year);
	st->wMonth = (WORD)(1 + tm_buf.tm_mon);
	st->wDayOfWeek = (WORD)tm_buf.tm_wday;
	st->wDay = (WORD)tm_buf.tm_mday;
	st->wHour = (WORD)tm_buf.tm_hour;
	st->wMinute = (WORD)tm_buf.tm_min;
	st->wSecond = (WORD)tm_buf.tm_sec;
	st->wMilliseconds = 0;
}

int GetLocaleInfoA(LCID, LCTYPE, LPSTR data, int cch)
{
	if (data && cch > 0) {
		data[0] = '0';
		if (cch > 1) {
			data[1] = '\0';
		}
	}
	return 1;
}

BOOL Renegade_GetKeyboardState(PBYTE keyState)
{
	if (keyState == NULL) {
		return FALSE;
	}
	for (int vk = 0; vk < 256; ++vk) {
		keyState[vk] = Platform_Get_Async_Key(vk) ? (BYTE)0x80 : (BYTE)0;
	}
	return TRUE;
}

DWORD GetTickCount(void) { return platform_tick_ms(); }

void InitializeCriticalSection(CRITICAL_SECTION *cs)
{
	if (!cs) {
		return;
	}
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&cs->mutex, &attr);
	pthread_mutexattr_destroy(&attr);
	cs->initialized = 1;
}

void EnterCriticalSection(CRITICAL_SECTION *cs)
{
	if (cs && cs->initialized) {
		pthread_mutex_lock(&cs->mutex);
	}
}

void LeaveCriticalSection(CRITICAL_SECTION *cs)
{
	if (cs && cs->initialized) {
		pthread_mutex_unlock(&cs->mutex);
	}
}

void DeleteCriticalSection(CRITICAL_SECTION *cs)
{
	if (cs && cs->initialized) {
		pthread_mutex_destroy(&cs->mutex);
		cs->initialized = 0;
	}
}

HANDLE CreateMutexA(void *, BOOL initial_owner, LPCSTR)
{
	SetLastError(0);
	return renegade_sync_create_event(TRUE, initial_owner);
}
BOOL ReleaseMutex(HANDLE h)
{
	return renegade_sync_set_event(h);
}

static int Renegade_Wide_Char_Count(LPCWSTR src, int srclen)
{
	if (src == NULL) {
		return 0;
	}
	if (srclen >= 0) {
		return srclen;
	}
	return (int)WW_WCSTRLEN(src);
}

static int Renegade_Wide_Code_Unit_To_Mb(wchar_t ch, char *out, int out_cap)
{
	mbstate_t state;
	memset(&state, 0, sizeof(state));
	char tmp[8];
	const size_t n = wcrtomb(tmp, ch, &state);
	if (n == (size_t)-1) {
		return -1;
	}
	if (out != NULL && out_cap > 0) {
		const int copy = (int)n < out_cap ? (int)n : out_cap;
		memcpy(out, tmp, (size_t)copy);
	}
	return (int)n;
}

int WideCharToMultiByte(
	UINT,
	DWORD,
	LPCWSTR src,
	int srclen,
	LPSTR dst,
	int dstlen,
	LPCSTR,
	LPBOOL used_default)
{
	if (src == NULL) {
		return 0;
	}
	if (srclen == 0) {
		return 0;
	}

	const bool include_null = (srclen < 0);
	const int wide_len = Renegade_Wide_Char_Count(src, srclen);
	bool unmapped = false;

	auto encode_unit = [&](wchar_t ch, char *out, int out_cap) -> int {
		if ((unsigned)ch < 0x80u) {
			if (out != NULL && out_cap > 0) {
				out[0] = (char)ch;
			}
			return 1;
		}
		const int n = Renegade_Wide_Code_Unit_To_Mb(ch, out, out_cap);
		if (n < 0) {
			unmapped = true;
			if (out != NULL && out_cap > 0) {
				out[0] = '?';
			}
			return 1;
		}
		return n;
	};

	int required = 0;
	for (int i = 0; i < wide_len; i++) {
		required += encode_unit((wchar_t)(unsigned short)src[i], NULL, 0);
	}
	if (include_null) {
		required += 1;
	}

	if (dst == NULL || dstlen <= 0) {
		if (used_default != NULL) {
			*used_default = unmapped ? TRUE : FALSE;
		}
		return required;
	}

	int out = 0;
	const int body_cap = include_null ? dstlen - 1 : dstlen;
	for (int i = 0; i < wide_len && out < body_cap; i++) {
		out += encode_unit(
			(wchar_t)(unsigned short)src[i],
			dst + out,
			body_cap - out);
	}
	if (include_null) {
		if (out < dstlen) {
			dst[out++] = '\0';
		}
		if (used_default != NULL) {
			*used_default = unmapped ? TRUE : FALSE;
		}
		return out > 0 ? out : 1;
	}
	if (used_default != NULL) {
		*used_default = unmapped ? TRUE : FALSE;
	}
	return out;
}

int MultiByteToWideChar(UINT, DWORD, LPCSTR src, int srclen, LPWSTR dst, int dstlen)
{
	if (src == NULL) {
		return 0;
	}
	if (srclen == 0) {
		return 0;
	}

	const bool include_null = (srclen < 0);
	int byte_len = srclen;
	if (include_null) {
		byte_len = (int)strlen(src);
	}

	if (dst == NULL || dstlen <= 0) {
		return include_null ? byte_len + 1 : byte_len;
	}

	const int wchar_cap = include_null ? dstlen - 1 : dstlen;
	int out = 0;
	for (int i = 0; i < byte_len && out < wchar_cap; i++) {
		dst[out++] = (WCHAR)(unsigned char)src[i];
	}
	if (include_null) {
		if (out < dstlen) {
			dst[out++] = L'\0';
		}
		return out > 0 ? out : 1;
	}
	return out;
}

/* FindResource* / LoadResource / LockResource / SizeofResource: pe_resource_loader.cpp */
HANDLE OpenMutexA(DWORD, BOOL, LPCSTR) { return NULL; }
DWORD GetProcessVersion(DWORD) { return 0; }

BOOL GetUserNameA(LPSTR buffer, LPDWORD size)
{
	if (!buffer || !size || *size == 0) {
		return FALSE;
	}
	const char *user = getenv("USER");
	if (!user || !user[0]) {
		user = "player";
	}
	size_t len = strlen(user);
	if (len >= *size) {
		len = *size - 1;
	}
	memcpy(buffer, user, len);
	buffer[len] = '\0';
	*size = (DWORD)len;
	return TRUE;
}

HANDLE OpenProcess(DWORD, BOOL, DWORD) { return INVALID_HANDLE_VALUE; }
BOOL TerminateProcess(HANDLE, UINT) { return FALSE; }
DWORD GetWindowThreadProcessId(HWND, LPDWORD pid)
{
	if (pid) {
		*pid = GetCurrentProcessId();
	}
	return GetCurrentThreadId();
}

UINT GetWindowsDirectoryA(LPSTR buffer, UINT size)
{
	if (!buffer || size == 0) {
		return 0;
	}
	const char *home = getenv("HOME");
	if (!home) {
		home = "/tmp";
	}
	strncpy(buffer, home, size - 1);
	buffer[size - 1] = '\0';
	return (UINT)strlen(buffer);
}

UINT GetTempFileNameA(LPCSTR path, LPCSTR prefix, UINT unique, LPSTR result)
{
	if (!result) {
		return 0;
	}
	snprintf(result, MAX_PATH, "%s/%s%03u.tmp", path ? path : "/tmp", prefix ? prefix : "ren",
		unique % 1000);
	return unique % 1000;
}

DWORD WaitForInputIdle(HANDLE, DWORD) { return 0; }
/* LoadResource / LockResource / SizeofResource: pe_resource_loader.cpp */

DWORD timeGetTime(void) { return platform_tick_ms(); }

MMRESULT timeBeginPeriod(UINT) { return TIMERR_NOERROR; }
MMRESULT timeEndPeriod(UINT) { return TIMERR_NOERROR; }

void OutputDebugStringA(LPCSTR str)
{
	if (str) {
		fputs(str, stderr);
	}
}

int ToAscii(UINT vk, UINT, PBYTE, LPWORD buffer, UINT)
{
	if (!buffer) {
		return 0;
	}
	const unsigned c = vk & 0xFF;
	if (c >= 32 && c < 127) {
		buffer[0] = (unsigned short)c;
		return 1;
	}
	return 0;
}

unsigned long _beginthread(ThreadFn start, unsigned, void *arg)
{
	return (unsigned long)_beginthreadex(NULL, 0, start, arg, 0, NULL);
}

uintptr_t _beginthreadex(void *, unsigned, ThreadFn start, void *arg, unsigned, unsigned *thrdaddr)
{
	const uintptr_t handle = renegade_sync_begin_thread(start, arg);
	if (handle == 0) {
		return 0;
	}
	if (thrdaddr) {
		*thrdaddr = (unsigned)handle;
	}
	return handle;
}

BOOL CreateDirectoryA(LPCSTR path, LPVOID)
{
	if (!path || !path[0]) {
		SetLastError(ERROR_PATH_NOT_FOUND);
		return FALSE;
	}

	if (renegade_path_looks_like_wide_leak(path)) {
		SetLastError(ERROR_PATH_NOT_FOUND);
		return FALSE;
	}

	return renegade_mkdir_p(path);
}

BOOL DeleteFileA(LPCSTR path)
{
	if (!path || !path[0]) {
		SetLastError(ERROR_PATH_NOT_FOUND);
		return FALSE;
	}
	char norm[MAX_PATH];
	renegade_path_normalize(norm, sizeof(norm), path);
	if (unlink(norm) != 0) {
		SetLastError((DWORD)errno);
		return FALSE;
	}
	return TRUE;
}

BOOL MoveFileA(LPCSTR from, LPCSTR to)
{
	if (!from || !to) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	char src[MAX_PATH];
	char dst[MAX_PATH];
	renegade_path_normalize(src, sizeof(src), from);
	renegade_path_normalize(dst, sizeof(dst), to);
	if (rename(src, dst) == 0) {
		return TRUE;
	}
	if (errno == EXDEV) {
		FILE *in = fopen(src, "rb");
		if (!in) {
			SetLastError((DWORD)errno);
			return FALSE;
		}
		FILE *out = fopen(dst, "wb");
		if (!out) {
			fclose(in);
			SetLastError((DWORD)errno);
			return FALSE;
		}
		char buf[4096];
		size_t n;
		while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
			if (fwrite(buf, 1, n, out) != n) {
				fclose(in);
				fclose(out);
				SetLastError(ERROR_WRITE_FAULT);
				return FALSE;
			}
		}
		fclose(in);
		fclose(out);
		if (unlink(src) != 0) {
			SetLastError((DWORD)errno);
			return FALSE;
		}
		return TRUE;
	}
	SetLastError((DWORD)errno);
	return FALSE;
}

void ExitProcess(UINT code)
{
	_exit((int)code);
}

BOOL GetComputerNameA(LPSTR buffer, LPDWORD size)
{
	const char *host = "linux";
	const DWORD need = (DWORD)(strlen(host) + 1);
	if (!buffer || !size || *size < need) {
		if (size) {
			*size = need;
		}
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return FALSE;
	}
	strcpy(buffer, host);
	*size = need - 1;
	return TRUE;
}

HANDLE GetCurrentProcess(void) { return (HANDLE)(intptr_t)getpid(); }

UINT GetDriveTypeA(LPCSTR root)
{
	(void)root;
	return DRIVE_FIXED;
}

BOOL GetExitCodeProcess(HANDLE, DWORD *code)
{
	if (code) {
		*code = STILL_ACTIVE;
	}
	return TRUE;
}

DWORD GetFileSize(HANDLE h, LPDWORD high)
{
	if (high) {
		*high = 0;
	}
	FILE *f = (FILE *)h;
	if (!f) {
		SetLastError(ERROR_INVALID_HANDLE);
		return INVALID_FILE_SIZE;
	}
	const long cur = ftell(f);
	if (cur < 0) {
		SetLastError(ERROR_INVALID_HANDLE);
		return INVALID_FILE_SIZE;
	}
	if (fseek(f, 0, SEEK_END) != 0) {
		SetLastError(ERROR_INVALID_HANDLE);
		return INVALID_FILE_SIZE;
	}
	const long end = ftell(f);
	fseek(f, cur, SEEK_SET);
	if (end < 0) {
		return INVALID_FILE_SIZE;
	}
	return (DWORD)end;
}

DWORD GetLogicalDriveStringsA(DWORD buflen, LPSTR buffer)
{
	if (!buffer || buflen < 4) {
		return 0;
	}
	buffer[0] = '/';
	buffer[1] = '\0';
	buffer[2] = '\0';
	return 2;
}

HMODULE GetModuleHandleA(LPCSTR)
{
	return (HMODULE)(intptr_t)1;
}

DWORD GetModuleFileNameA(HMODULE, LPSTR filename, DWORD size)
{
	if (!filename || size == 0) {
		return 0;
	}
	ssize_t n = readlink("/proc/self/exe", filename, size - 1);
	if (n < 0) {
		strncpy(filename, "./renegade", size - 1);
		filename[size - 1] = '\0';
		return (DWORD)strlen(filename);
	}
	filename[n] = '\0';
	return (DWORD)n;
}

BOOL GlobalMemoryStatus(LPMEMORYSTATUS status)
{
	if (!status) {
		return FALSE;
	}
	memset(status, 0, sizeof(*status));
	status->dwLength = sizeof(*status);
	struct sysinfo info;
	if (sysinfo(&info) == 0) {
		status->dwTotalPhys = (DWORD)(info.totalram * info.mem_unit);
		status->dwAvailPhys = (DWORD)(info.freeram * info.mem_unit);
		status->dwMemoryLoad =
			status->dwTotalPhys > 0
				? (DWORD)(100 - (status->dwAvailPhys * 100 / status->dwTotalPhys))
				: 0;
	}
	return TRUE;
}

BOOL GetVersionExA(OSVERSIONINFOA *info)
{
	if (!info) {
		return FALSE;
	}
	memset(info, 0, sizeof(*info));
	info->dwOSVersionInfoSize = sizeof(*info);
	info->dwMajorVersion = 5;
	info->dwMinorVersion = 0;
	info->dwBuildNumber = 0;
	info->dwPlatformId = VER_PLATFORM_WIN32_NT;
	strncpy(info->szCSDVersion, "Linux", sizeof(info->szCSDVersion) - 1);
	return TRUE;
}

DWORD GetTimeZoneInformation(TIME_ZONE_INFORMATION *tz)
{
	if (tz) {
		memset(tz, 0, sizeof(*tz));
	}
	return 0;
}

BOOL GetVolumeInformationA(
	LPCSTR root, LPSTR vol, DWORD volSize, LPDWORD serial,
	LPDWORD maxComp, LPDWORD flags, LPSTR fs, DWORD fsSize)
{
	(void)root;
	if (vol && volSize > 0) {
		strncpy(vol, "ROOT", volSize - 1);
		vol[volSize - 1] = '\0';
	}
	if (serial) {
		*serial = 0;
	}
	if (maxComp) {
		*maxComp = 255;
	}
	if (flags) {
		*flags = 0;
	}
	if (fs && fsSize > 0) {
		strncpy(fs, "ext4", fsSize - 1);
		fs[fsSize - 1] = '\0';
	}
	return TRUE;
}

BOOL QueryPerformanceFrequency(LARGE_INTEGER *freq)
{
	if (!freq) {
		return FALSE;
	}
	freq->QuadPart = 1000000000LL;
	return TRUE;
}

BOOL QueryPerformanceCounter(LARGE_INTEGER *counter)
{
	if (!counter) {
		return FALSE;
	}
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	counter->QuadPart =
		(long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
	return TRUE;
}

void _splitpath(const char *path, char *drive, char *dir, char *fname, char *ext)
{
	if (drive) {
		drive[0] = '\0';
	}
	if (!path) {
		if (dir) {
			dir[0] = '\0';
		}
		if (fname) {
			fname[0] = '\0';
		}
		if (ext) {
			ext[0] = '\0';
		}
		return;
	}
	const char *base = strrchr(path, '/');
	if (!base) {
		base = strrchr(path, '\\');
	}
	if (!base) {
		base = path;
	} else {
		base++;
		if (dir) {
			size_t len = (size_t)(base - path);
			if (len >= (size_t)_MAX_DIR) {
				len = (size_t)_MAX_DIR - 1;
			}
			memcpy(dir, path, len);
			dir[len] = '\0';
		}
	}
	if (dir && base == path) {
		dir[0] = '\0';
	}
	const char *dot = strrchr(base, '.');
	if (fname) {
		if (dot && dot > base) {
			size_t len = (size_t)(dot - base);
			if (len >= (size_t)_MAX_FNAME) {
				len = (size_t)_MAX_FNAME - 1;
			}
			memcpy(fname, base, len);
			fname[len] = '\0';
		} else {
			strncpy(fname, base, _MAX_FNAME - 1);
			fname[_MAX_FNAME - 1] = '\0';
		}
	}
	if (ext) {
		if (dot && dot > base) {
			strncpy(ext, dot, _MAX_EXT - 1);
			ext[_MAX_EXT - 1] = '\0';
		} else {
			ext[0] = '\0';
		}
	}
}

void _makepath(char *path, const char *drive, const char *dir, const char *fname, const char *ext)
{
	if (!path) {
		return;
	}
	path[0] = '\0';
	(void)drive;
	if (dir && dir[0]) {
		strcat(path, dir);
		size_t len = strlen(path);
		if (len > 0 && path[len - 1] != '/' && path[len - 1] != '\\') {
			strcat(path, "/");
		}
	}
	if (fname && fname[0]) {
		strcat(path, fname);
	}
	if (ext && ext[0]) {
		if (ext[0] != '.') {
			strcat(path, ".");
		}
		strcat(path, ext);
	}
}

static void linux_copy_module_basename_lower(const char *name, char *out, size_t out_size)
{
	const char *base = strrchr(name, '/');
	base = base ? base + 1 : name;
	const char *base_bs = strrchr(base, '\\');
	if (base_bs != NULL) {
		base = base_bs + 1;
	}

	size_t i = 0;
	while (base[i] != '\0' && i + 1 < out_size) {
		out[i] = (char)tolower((unsigned char)base[i]);
		i++;
	}
	out[i] = '\0';

	char *dot = strrchr(out, '.');
	if (dot != NULL) {
		strcpy(dot, ".so");
	} else if (i + 3 < out_size) {
		strcat(out, ".so");
	}
}

static void linux_build_module_path(const char *dir, const char *file, char *out, size_t out_size)
{
	if (dir == NULL || dir[0] == '\0') {
		strncpy(out, file, out_size - 1);
	} else {
		snprintf(out, out_size, "%s%s", dir, file);
	}
	out[out_size - 1] = '\0';
}

static HMODULE linux_try_dlopen(const char *path)
{
	if (path == NULL || path[0] == '\0') {
		return NULL;
	}

	void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	return handle != NULL ? (HMODULE)handle : NULL;
}

HMODULE LoadLibraryA(LPCSTR name)
{
	if (name == NULL || name[0] == '\0') {
		SetLastError(ERROR_MOD_NOT_FOUND);
		return NULL;
	}

	char exe_dir[PATH_MAX];
	char module_file[PATH_MAX];
	char try_path[PATH_MAX];
	exe_dir[0] = '\0';

	const bool has_dir =
		(strchr(name, '/') != NULL) ||
		(strchr(name, '\\') != NULL);

	if (!has_dir) {
		char exe_path[PATH_MAX];
		DWORD n = GetModuleFileNameA(NULL, exe_path, MAX_PATH);
		if (n > 0 && n < MAX_PATH) {
			strncpy(exe_dir, exe_path, sizeof(exe_dir) - 1);
			exe_dir[sizeof(exe_dir) - 1] = '\0';
			char *slash = strrchr(exe_dir, '/');
			if (slash != NULL) {
				slash[1] = '\0';
			}
		}
	}

	linux_copy_module_basename_lower(name, module_file, sizeof(module_file));

	const char *candidates[] = {
		name,
		module_file,
		NULL,
	};

	for (int pass = 0; pass < 2; ++pass) {
		const char *dir = (pass == 0 && !has_dir) ? exe_dir : "";
		for (int i = 0; i < 2; ++i) {
			if (candidates[i] == NULL) {
				continue;
			}
			linux_build_module_path(dir, candidates[i], try_path, sizeof(try_path));
			HMODULE mod = linux_try_dlopen(try_path);
			if (mod != NULL) {
				return mod;
			}
		}
	}

	SetLastError(ERROR_MOD_NOT_FOUND);
	return NULL;
}

void *GetProcAddress(HMODULE mod, LPCSTR name)
{
	if (mod == NULL || name == NULL) {
		SetLastError(ERROR_PROC_NOT_FOUND);
		return NULL;
	}

	// GetModuleHandleA() returns this sentinel on Linux — not a real dl handle.
	if (mod == (HMODULE)(intptr_t)1) {
		SetLastError(ERROR_PROC_NOT_FOUND);
		return NULL;
	}

	void *sym = dlsym(mod, name);
	if (sym == NULL) {
		SetLastError(ERROR_PROC_NOT_FOUND);
	}
	return sym;
}

BOOL FreeLibrary(HMODULE mod)
{
	if (mod == NULL) {
		return FALSE;
	}
	if (dlclose(mod) != 0) {
		return FALSE;
	}
	return TRUE;
}

BOOL SetThreadPriority(HANDLE, int) { return TRUE; }

static int g_show_cursor_count = -1;

int ShowCursor(BOOL show)
{
	if (show) {
		g_show_cursor_count++;
	} else {
		g_show_cursor_count--;
	}

	SDL_Window *window = Platform_Get_SDL_Window();
	if (window != NULL) {
		if (g_show_cursor_count >= 0) {
			SDL_ShowCursor();
			SDL_SetWindowRelativeMouseMode(window, false);
		} else {
			SDL_HideCursor();
		}
	}

	return g_show_cursor_count;
}

BOOL GetCursorPos(LPPOINT pt)
{
	if (pt == NULL) {
		return FALSE;
	}

	float x = 0.0f;
	float y = 0.0f;
	SDL_GetMouseState(&x, &y);
	pt->x = (LONG)x;
	pt->y = (LONG)y;
	return TRUE;
}

BOOL SetCursorPos(int x, int y)
{
	SDL_Window *window = Platform_Get_SDL_Window();
	if (window == NULL) {
		return FALSE;
	}
	SDL_WarpMouseInWindow(window, (float)x, (float)y);
	return TRUE;
}

BOOL TerminateThread(HANDLE, DWORD) { return FALSE; }

HRESULT CoCreateInstance(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID *ppv)
{
	if (ppv) {
		*ppv = NULL;
	}
	return (HRESULT)0x80040111L; /* CLASS_E_CLASSNOTREG */
}

HRESULT CoInitialize(LPVOID)
{
	return S_OK;
}

void CoUninitialize(void)
{
}

LONG InterlockedIncrement(LONG volatile *v)
{
	return __sync_add_and_fetch(v, 1);
}

LONG InterlockedDecrement(LONG volatile *v)
{
	return __sync_sub_and_fetch(v, 1);
}
