/*
 * Win32 supplement for Renegade on Linux (SDL3 + dxvk-native).
 * Base types come from dxvk <windows.h>; this header adds UI/message APIs.
 */
#ifndef RENEGADE_WIN32_MINIMAL_H
#define RENEGADE_WIN32_MINIMAL_H

#include <windows.h>
#include "winuser_extra.h"

#ifndef CALLBACK
#define CALLBACK WINAPI
#endif
#ifndef PASCAL
#define PASCAL WINAPI
#endif

typedef BYTE *PBYTE;
typedef WORD *LPWORD;

#ifndef WPARAM
typedef UINT_PTR WPARAM;
typedef UINT_PTR LPARAM;
typedef LONG_PTR LRESULT;
#endif

#ifndef ULARGE_INTEGER
typedef union _ULARGE_INTEGER {
	struct {
		DWORD LowPart;
		DWORD HighPart;
	};
	unsigned long long QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;
#endif

typedef HANDLE HACCEL;
typedef HANDLE HICON;
typedef HANDLE HCURSOR;
typedef HANDLE HBRUSH;
typedef HANDLE HGLRC;
#ifndef MB_OK
#define MB_OK 0
#define MB_ICONWARNING 0x30
#define MB_OKCANCEL 0x1
#endif

#ifndef CS_HREDRAW
#define CS_HREDRAW 0x0001
#define CS_VREDRAW 0x0002
#define CS_DBLCLKS 0x0008
#endif

#ifndef WS_SYSMENU
#define WS_SYSMENU 0x00080000L
#define WS_CAPTION 0x00C00000L
#define WS_MINIMIZEBOX 0x00020000L
#define WS_CLIPCHILDREN 0x02000000L
#define WS_VISIBLE 0x10000000L
#define WS_CHILD 0x40000000L
#define WS_DISABLED 0x08000000L
#define WS_GROUP 0x00020000L
#endif

#ifndef IDOK
#define IDOK 1
#define IDCANCEL 2
#endif

#ifndef SS_TYPEMASK
#define SS_TYPEMASK 0x0000001FL
#define SS_LEFT 0x00000000L
#define SS_CENTER 0x00000001L
#define SS_RIGHT 0x00000002L
#define SS_ICON 0x00000003L
#define SS_BLACKRECT 0x00000004L
#define SS_GRAYRECT 0x00000005L
#define SS_WHITERECT 0x00000006L
#define SS_BLACKFRAME 0x00000007L
#define SS_GRAYFRAME 0x00000008L
#define SS_WHITEFRAME 0x00000009L
#define SS_ETCHEDHORZ 0x00000010L
#define SS_ETCHEDVERT 0x00000011L
#define SS_ETCHEDFRAME 0x00000012L
#define SS_CENTERIMAGE 0x00000200L
#define SS_LEFTNOWORDWRAP 0x0000000CL
#define SS_BITMAP 0x0000000EL
#endif

#ifndef WM_ACTIVATEAPP
#define WM_ACTIVATEAPP 0x001C
#define WM_ERASEBKGND 0x0014
#define WM_PAINT 0x000F
#define WM_SYSKEYDOWN 0x0104
#define WM_CHAR 0x0102
#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_KEYDOWN 0x0100
#define WM_KEYUP 0x0101
#define WM_SYSKEYUP 0x0105
#define WM_QUIT 0x0012
#define WM_MOUSEMOVE 0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_RBUTTONDOWN 0x0204
#define WM_RBUTTONUP 0x0205
#define WM_MBUTTONDOWN 0x0207
#define WM_MBUTTONUP 0x0208
#define WM_LBUTTONDBLCLK 0x0203
#define WM_RBUTTONDBLCLK 0x0206
#define WM_MBUTTONDBLCLK 0x0209
#endif

#ifndef DWORD_PTR
typedef ULONG_PTR DWORD_PTR;
#endif
#ifndef LOWORD
#define LOWORD(l) ((WORD)((DWORD_PTR)(l) & 0xFFFF))
#define HIWORD(l) ((WORD)(((DWORD_PTR)(l) >> 16) & 0xFFFF))
#endif

#ifndef WS_BORDER
#define WS_BORDER 0x00800000L
#endif

#ifndef BN_CLICKED
#define BN_CLICKED 0
#define BN_DOUBLECLICKED 5
#endif

#ifndef BS_BITMAP
#define BS_BITMAP 0x00000080L
#define BS_PUSHBUTTON 0x00000000L
#define BS_DEFPUSHBUTTON 0x00000001L
#define BS_CHECKBOX 0x00000002L
#define BS_AUTOCHECKBOX 0x00000003L
#define BS_AUTORADIOBUTTON 0x00000040L
#define BS_FLAT 0x00008000L
#define BS_OWNERDRAW 0x0000000BL
#define BS_LEFT 0x00000100L
#endif

#ifndef VK_RETURN
#define VK_RETURN 0x0D
#define VK_SPACE 0x20
#define VK_ESCAPE 0x1B
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_CAPITAL 0x14
#define VK_NUMLOCK 0x90
#endif

#ifndef KF_ALTDOWN
#define KF_ALTDOWN 0x2000
#define KF_REPEAT 0x4000
#endif

#ifndef PM_NOREMOVE
#define PM_NOREMOVE 0x0000
#define PM_REMOVE 0x0001
#endif

#ifndef BLACK_BRUSH
#define BLACK_BRUSH 4
#define IDC_ARROW ((LPCSTR)32512)
#define IDI_APPLICATION ((LPCSTR)32512)
#endif

#ifndef STILL_ACTIVE
#define STILL_ACTIVE 259
#endif

#ifndef tagMSG
struct tagMSG {
	HWND hwnd;
	UINT message;
	WPARAM wParam;
	LPARAM lParam;
	DWORD time;
	POINT pt;
};
typedef struct tagMSG MSG;
#endif

#ifndef tagWNDCLASS
typedef LRESULT (WINAPI *WNDPROC)(HWND, UINT, WPARAM, LPARAM);
struct tagWNDCLASS {
	UINT style;
	WNDPROC lpfnWndProc;
	int cbClsExtra;
	int cbWndExtra;
	HINSTANCE hInstance;
	HICON hIcon;
	HCURSOR hCursor;
	HBRUSH hbrBackground;
	LPCSTR lpszMenuName;
	LPCSTR lpszClassName;
};
typedef struct tagWNDCLASS WNDCLASS;
#endif

#ifndef tagSTARTUPINFO
struct tagSTARTUPINFO {
	DWORD cb;
};
typedef struct tagSTARTUPINFO STARTUPINFO;
#endif

#ifndef tagPROCESS_INFORMATION
struct tagPROCESS_INFORMATION {
	HANDLE hProcess;
	HANDLE hThread;
	DWORD dwProcessId;
	DWORD dwThreadId;
};
typedef struct tagPROCESS_INFORMATION PROCESS_INFORMATION;
#endif

#ifndef _CRITICAL_SECTION_DEFINED
#define _CRITICAL_SECTION_DEFINED
#include <pthread.h>
typedef struct _CRITICAL_SECTION {
	pthread_mutex_t mutex;
	int initialized;
} CRITICAL_SECTION, *PCRITICAL_SECTION;
#endif

#ifdef __cplusplus
extern "C" {
#endif

HWND GetDesktopWindow(void);
BOOL SetFocus(HWND hwnd);
int MessageBoxA(HWND hwnd, LPCSTR text, LPCSTR caption, UINT type);
#define MessageBox MessageBoxA
#ifndef MB_ICONSTOP
#define MB_ICONSTOP MB_ICONWARNING
#endif
BOOL RegisterClassA(const WNDCLASS *wc);
HWND CreateWindowExA(DWORD exStyle, LPCSTR className, LPCSTR windowName, DWORD style,
	int x, int y, int w, int h, HWND parent, HANDLE menu, HINSTANCE instance, LPVOID param);
BOOL ValidateRect(HWND hwnd, const RECT *rect);
void ReleaseCapture(void);
void PostQuitMessage(int code);
BOOL PeekMessageA(MSG *msg, HWND hwnd, UINT filterMin, UINT filterMax, UINT remove);
BOOL GetMessageA(MSG *msg, HWND hwnd, UINT filterMin, UINT filterMax);
BOOL TranslateMessage(const MSG *msg);
LRESULT DispatchMessageA(const MSG *msg);
BOOL TranslateAcceleratorA(HWND hwnd, HACCEL accel, MSG *msg);
BOOL IsDialogMessageA(HWND dlg, MSG *msg);
#define PeekMessage PeekMessageA
#define GetMessage GetMessageA
#define TranslateAccelerator TranslateAcceleratorA
#define IsDialogMessage IsDialogMessageA
#define DispatchMessage DispatchMessageA
SHORT GetAsyncKeyState(int vkey);
SHORT GetKeyState(int vkey);
int MapVirtualKeyA(UINT code, UINT maptype);
#define MapVirtualKey MapVirtualKeyA
BOOL ClientToScreen(HWND hwnd, POINT *pt);
LRESULT DefWindowProcA(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#define DefWindowProc DefWindowProcA
void Sleep(DWORD ms);
void InitializeCriticalSection(CRITICAL_SECTION *cs);
void EnterCriticalSection(CRITICAL_SECTION *cs);
void LeaveCriticalSection(CRITICAL_SECTION *cs);
void DeleteCriticalSection(CRITICAL_SECTION *cs);
HANDLE CreateMutexA(void *attr, BOOL initial_owner, LPCSTR name);
BOOL ReleaseMutex(HANDLE mutex);
BOOL SearchPathA(LPCSTR path, LPCSTR file, LPCSTR ext, DWORD buflen, CHAR *out, CHAR **filepart);
BOOL CreateProcessA(LPCSTR app, LPSTR cmdline, LPVOID a, LPVOID b, BOOL inherit, DWORD flags,
	LPVOID env, LPCSTR dir, STARTUPINFO *si, PROCESS_INFORMATION *pi);
#define CreateProcess CreateProcessA
HICON LoadIconA(HINSTANCE inst, LPCSTR name);
HCURSOR LoadCursorA(HINSTANCE inst, LPCSTR name);
HBRUSH GetStockObject(int obj);
void SetUnhandledExceptionFilter(void *filter);

#ifdef __cplusplus
}
#endif

#endif /* RENEGADE_WIN32_MINIMAL_H */
