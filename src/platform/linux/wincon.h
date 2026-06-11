#ifndef RENEGADE_WINCON_H
#define RENEGADE_WINCON_H

#include <windows.h>
#include "winuser.h"

typedef struct _COORD {
	SHORT X;
	SHORT Y;
} COORD;

typedef struct _CONSOLE_SCREEN_BUFFER_INFO {
	WORD wAttributes;
	COORD dwSize;
	COORD dwCursorPosition;
} CONSOLE_SCREEN_BUFFER_INFO;

#define FOREGROUND_BLUE 0x0001
#define FOREGROUND_GREEN 0x0002
#define FOREGROUND_RED 0x0004
#define FOREGROUND_INTENSITY 0x0008
#define BACKGROUND_BLUE 0x0010
#define BACKGROUND_GREEN 0x0020
#define BACKGROUND_RED 0x0040
#define BACKGROUND_INTENSITY 0x0080

#ifdef __cplusplus
extern "C" {
#endif

BOOL SetConsoleScreenBufferSize(HANDLE h, COORD size);
BOOL FillConsoleOutputAttribute(HANDLE h, WORD attr, DWORD len, COORD pos, LPDWORD written);
BOOL SetConsoleTextAttribute(HANDLE h, WORD attr);
BOOL SetConsoleTitleA(LPCSTR title);
#define SetConsoleTitle SetConsoleTitleA
BOOL GetConsoleScreenBufferInfo(HANDLE h, CONSOLE_SCREEN_BUFFER_INFO *info);
BOOL SetConsoleCursorPosition(HANDLE h, COORD pos);
BOOL FillConsoleOutputCharacterA(HANDLE h, CHAR ch, DWORD len, COORD pos, LPDWORD written);
#define FillConsoleOutputCharacter FillConsoleOutputCharacterA
void GetLocalTime(LPSYSTEMTIME st);
BOOL SystemTimeToFileTime(const SYSTEMTIME *st, LPFILETIME ft);

#ifdef __cplusplus
}
#endif

#endif
