#ifndef RENEGADE_WINRES_H
#define RENEGADE_WINRES_H

#include <windows.h>
#include "renegade_win32_shim.h"

#ifndef MAKEINTRESOURCE
#define MAKEINTRESOURCE(i) ((LPCSTR)(ULONG_PTR)((WORD)(i)))
#endif

#define RT_CURSOR MAKEINTRESOURCE(1)
#define RT_BITMAP MAKEINTRESOURCE(2)
#define RT_ICON MAKEINTRESOURCE(3)
#define RT_MENU MAKEINTRESOURCE(4)
#define RT_DIALOG MAKEINTRESOURCE(5)
#define RT_STRING MAKEINTRESOURCE(6)
#define RT_FONTDIR MAKEINTRESOURCE(7)
#define RT_FONT MAKEINTRESOURCE(8)
#define RT_ACCELERATOR MAKEINTRESOURCE(9)
#define RT_RCDATA MAKEINTRESOURCE(10)

#ifndef LANG_NEUTRAL
#define LANG_NEUTRAL 0x00
#endif
#ifndef SUBLANG_NEUTRAL
#define SUBLANG_NEUTRAL 0x00
#endif
#ifndef MAKELANGID
#define MAKELANGID(p, s) ((((WORD)(s)) << 10) | (WORD)(p))
#endif

#ifdef __cplusplus
extern "C" {
#endif
HRSRC FindResourceA(HMODULE hModule, LPCSTR lpName, LPCSTR lpType);
HRSRC FindResourceExA(HMODULE hModule, LPCSTR lpType, LPCSTR lpName, WORD wLanguage);
HGLOBAL LoadResource(HMODULE hModule, HRSRC hResInfo);
void *LockResource(HGLOBAL hResData);
DWORD SizeofResource(HMODULE hModule, HRSRC hResInfo);
#ifdef __cplusplus
}
#endif
#define FindResourceEx FindResourceExA

#endif
