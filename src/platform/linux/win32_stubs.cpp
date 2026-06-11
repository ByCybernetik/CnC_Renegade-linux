/*
 * Stubs for Win32 APIs used before full port coverage (no real window class on Linux).
 */
#include "win32_minimal.h"
#include "sdl3_host.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

HWND GetDesktopWindow(void) { return Platform_Get_Main_HWnd(); }
BOOL SetFocus(HWND) { return TRUE; }

int MessageBoxA(HWND, LPCSTR text, LPCSTR caption, UINT)
{
	fprintf(stderr, "%s: %s\n", caption ? caption : "Message", text ? text : "");
	return 0;
}

BOOL RegisterClassA(const WNDCLASS *) { return TRUE; }

HWND CreateWindowExA(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int,
	HWND, HANDLE, HINSTANCE, LPVOID)
{
	return Platform_Get_Main_HWnd();
}

BOOL ValidateRect(HWND, const RECT *) { return TRUE; }

BOOL ClientToScreen(HWND, POINT *pt)
{
	return pt != NULL;
}

LRESULT DefWindowProcA(HWND, UINT, WPARAM, LPARAM) { return 0; }
void ReleaseCapture(void) {}
extern "C" void Renegade_Stop_Main_Loop(int exitCode);

void PostQuitMessage(int exitCode)
{
	Renegade_Stop_Main_Loop(exitCode);
}

BOOL PeekMessageA(MSG *msg, HWND, UINT, UINT, UINT remove)
{
	(void)remove;
	if (!msg) {
		return FALSE;
	}
	memset(msg, 0, sizeof(*msg));
	return FALSE;
}

BOOL GetMessageA(MSG *msg, HWND, UINT, UINT)
{
	(void)msg;
	Platform_Pump_Events();
	return FALSE;
}

BOOL TranslateMessage(const MSG *) { return TRUE; }
LRESULT DispatchMessageA(const MSG *) { return 0; }
HACCEL LoadAcceleratorsA(HMODULE, LPCSTR) { return NULL; }
BOOL TranslateAcceleratorA(HWND, HACCEL, MSG *) { return FALSE; }
BOOL IsDialogMessageA(HWND, MSG *) { return FALSE; }

SHORT GetAsyncKeyState(int vkey)
{
	return Platform_Get_Async_Key(vkey) ? (SHORT)0x8000 : 0;
}

void Sleep(DWORD ms) { SDL_Delay(ms); }

BOOL SearchPathA(LPCSTR path, LPCSTR file, LPCSTR, DWORD buflen, CHAR *out, CHAR **filepart)
{
	if (!file || !out || buflen < 2) {
		return FALSE;
	}
	const char *base = (path && path[0]) ? path : ".";
	snprintf(out, buflen, "%s/%s", base, file);
	for (char *p = out; *p; ++p) {
		if (*p == '\\') {
			*p = '/';
		}
	}
	struct stat st;
	if (stat(out, &st) != 0 || !S_ISREG(st.st_mode)) {
		return FALSE;
	}
	if (filepart) {
		char *slash = strrchr(out, '/');
		*filepart = slash ? slash + 1 : out;
	}
	return TRUE;
}

BOOL CreateProcessA(LPCSTR, LPSTR, LPVOID, LPVOID, BOOL, DWORD,
	LPVOID, LPCSTR, STARTUPINFO *, PROCESS_INFORMATION *)
{
	return FALSE;
}

HICON LoadIconA(HINSTANCE, LPCSTR) { return NULL; }
HCURSOR LoadCursorA(HINSTANCE, LPCSTR) { return NULL; }
HBRUSH GetStockObject(int) { return NULL; }
void SetUnhandledExceptionFilter(void *) {}

#ifdef __cplusplus
}
#endif
