#ifndef RENEGADE_WINUSER_EXTRA_H
#define RENEGADE_WINUSER_EXTRA_H

#include <windows.h>

#ifndef VK_LBUTTON
#define VK_LBUTTON 0x01
#define VK_RBUTTON 0x02
#define VK_CANCEL 0x03
#define VK_MBUTTON 0x04
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_CLEAR 0x0C
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_PAUSE 0x13
#define VK_CAPITAL 0x14
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_SELECT 0x29
#define VK_PRINT 0x2A
#define VK_EXECUTE 0x2B
#define VK_SNAPSHOT 0x2C
#define VK_INSERT 0x2D
#define VK_DELETE 0x2E
#define VK_HELP 0x2F
#define VK_F1 0x70
#define VK_F2 0x71
#define VK_F3 0x72
#define VK_F4 0x73
#define VK_F5 0x74
#define VK_F6 0x75
#define VK_F7 0x76
#define VK_F8 0x77
#define VK_F9 0x78
#define VK_F10 0x79
#define VK_F11 0x7A
#define VK_F12 0x7B
#endif

#ifndef WM_SYSCOMMAND
#define WM_SYSCOMMAND 0x0112
#endif
#ifndef SC_CLOSE
#define SC_CLOSE 0xF060
#endif
#ifndef SC_KEYMENU
#define SC_KEYMENU 0xF100
#endif
#ifndef SC_SCREENSAVE
#define SC_SCREENSAVE 0xF140
#endif
#ifndef WM_COMMAND
#define WM_COMMAND 0x0111
#endif

#ifndef IDOK
#define IDOK 1
#define IDCANCEL 2
#define IDYES 6
#define IDNO 7
#endif

#ifndef CBS_DROPDOWN
#define CBS_DROPDOWN 0x0002
#define CBS_DROPDOWNLIST 0x0003
#define CBS_OEMCONVERT 0x0080
#endif

#ifndef ES_OEMCONVERT
#define ES_OEMCONVERT 0x0400
#define ES_AUTOHSCROLL 0x0080
#define ES_READONLY 0x0800
#define ES_PASSWORD 0x0020
#define ES_NUMBER 0x2000
#define ES_MULTILINE 0x0004
#define ES_AUTOVSCROLL 0x0040
#define ES_CENTER 0x0001
#endif

#ifndef SPI_GETWHEELSCROLLLINES
#define SPI_GETWHEELSCROLLLINES 0x0068
#endif
#ifndef WHEEL_DELTA
#define WHEEL_DELTA 120
#endif

#ifndef WM_INPUTLANGCHANGE
#define WM_INPUTLANGCHANGE 0x0051
#define WM_INPUTLANGCHANGEREQUEST 0x0050
#endif

#define ISC_SHOWUIALL 0x0000000F

#ifndef LANG_NEUTRAL
#define LANG_NEUTRAL 0x00
#define SUBLANG_DEFAULT 0x01
#define SORT_DEFAULT 0x0
#define MAKELANGID(p, s) ((DWORD)(((WORD)(s) << 10) | (WORD)(p)))
#define MAKELCID(l, s) ((DWORD)((((DWORD)((WORD)(s))) << 16) | (WORD)(l)))
#define LOCALE_IDEFAULTANSICODEPAGE 0x00001004
#endif

typedef void *HKL;
typedef DWORD LCID;
typedef DWORD LCTYPE;

#ifdef __cplusplus
extern "C" {
#endif

HKL GetKeyboardLayout(DWORD thread);
int GetKeyboardLayoutList(int count, HKL *list);
int GetLocaleInfoA(LCID locale, LCTYPE type, LPSTR data, int cch);
#define GetLocaleInfo GetLocaleInfoA

#ifdef __cplusplus
}
#endif

#define IME_CAND_CODE 0x0001

#ifndef PBYTE
typedef BYTE *PBYTE;
#endif

#ifdef __cplusplus
extern "C" {
#endif

BOOL Renegade_GetKeyboardState(PBYTE keyState);
#define GetKeyboardState Renegade_GetKeyboardState

#ifdef __cplusplus
}
#endif

#endif
