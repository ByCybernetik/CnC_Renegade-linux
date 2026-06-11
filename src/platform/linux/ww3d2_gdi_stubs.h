#ifndef WW3D2_GDI_STUBS_H
#define WW3D2_GDI_STUBS_H

#include <windows.h>

#ifndef _MAX_FNAME
#define _MAX_FNAME 256
#endif
#ifndef _MAX_EXT
#define _MAX_EXT 256
#endif

typedef HANDLE HFONT;
typedef HANDLE HBITMAP;
typedef HANDLE HGDIOBJ;

typedef struct tagTEXTMETRICW {
	LONG tmHeight;
	LONG tmAscent;
	LONG tmDescent;
	LONG tmInternalLeading;
	LONG tmExternalLeading;
	LONG tmAveCharWidth;
	LONG tmMaxCharWidth;
	LONG tmWeight;
	LONG tmOverhang;
	LONG tmDigitizedAspectX;
	LONG tmDigitizedAspectY;
	WCHAR tmFirstChar;
	WCHAR tmLastChar;
	WCHAR tmDefaultChar;
	WCHAR tmBreakChar;
	BYTE tmItalic;
	BYTE tmUnderlined;
	BYTE tmStruckOut;
	BYTE tmPitchAndFamily;
	BYTE tmCharSet;
} TEXTMETRICW, *PTEXTMETRICW, *LPTEXTMETRICW;
#define TEXTMETRIC TEXTMETRICW
#define LPTEXTMETRIC LPTEXTMETRICW

typedef struct tagBITMAPINFOHEADER {
	DWORD biSize;
	LONG biWidth;
	LONG biHeight;
	WORD biPlanes;
	WORD biBitCount;
	DWORD biCompression;
	DWORD biSizeImage;
	LONG biXPelsPerMeter;
	LONG biYPelsPerMeter;
	DWORD biClrUsed;
	DWORD biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER, *LPBITMAPINFOHEADER;

typedef struct tagRGBQUAD {
	BYTE rgbBlue;
	BYTE rgbGreen;
	BYTE rgbRed;
	BYTE rgbReserved;
} RGBQUAD;

typedef struct tagBITMAPINFO {
	BITMAPINFOHEADER bmiHeader;
	RGBQUAD bmiColors[1];
} BITMAPINFO, *PBITMAPINFO, *LPBITMAPINFO;

#ifndef BI_RGB
#define BI_RGB 0
#endif
#ifndef DIB_RGB_COLORS
#define DIB_RGB_COLORS 0
#endif
#ifndef ETO_OPAQUE
#define ETO_OPAQUE 0x0002
#endif
#ifndef FW_BOLD
#define FW_BOLD 700
#endif
#ifndef FW_NORMAL
#define FW_NORMAL 400
#endif
#ifndef LOGPIXELSY
#define LOGPIXELSY 90
#endif
#ifndef OUT_DEFAULT_PRECIS
#define OUT_DEFAULT_PRECIS 0
#endif
#ifndef CLIP_DEFAULT_PRECIS
#define CLIP_DEFAULT_PRECIS 0
#endif
#ifndef ANTIALIASED_QUALITY
#define ANTIALIASED_QUALITY 4
#endif
#ifndef VARIABLE_PITCH
#define VARIABLE_PITCH 0x04
#endif
#ifndef DEFAULT_CHARSET
#define DEFAULT_CHARSET 1
#endif
#ifndef CHINESEBIG5_CHARSET
#define CHINESEBIG5_CHARSET 136
#endif
#ifndef SHIFTJIS_CHARSET
#define SHIFTJIS_CHARSET 128
#endif
#ifndef HANGUL_CHARSET
#define HANGUL_CHARSET 129
#endif

HDC WINAPI GetDC(HWND hwnd);
int WINAPI ReleaseDC(HWND hwnd, HDC hdc);
BOOL WINAPI GetTextExtentPoint32W(HDC hdc, LPCWSTR str, int count, LPSIZE size);
BOOL WINAPI ExtTextOutW(HDC hdc, int x, int y, UINT options, const RECT *rect, LPCWSTR str, UINT count, const INT *dx);
int WINAPI MulDiv(int n, int numerator, int denominator);
int WINAPI GetDeviceCaps(HDC hdc, int index);
UINT WINAPI GetACP(void);
HFONT WINAPI CreateFontW(int height, int width, int escapement, int orientation, int weight, DWORD italic,
	DWORD underline, DWORD strikeout, DWORD charset, DWORD output_precision, DWORD clip_precision,
	DWORD quality, DWORD pitch_and_family, LPCWSTR face);
HFONT WINAPI CreateFontA(int height, int width, int escapement, int orientation, int weight, DWORD italic,
	DWORD underline, DWORD strikeout, DWORD charset, DWORD output_precision, DWORD clip_precision,
	DWORD quality, DWORD pitch_and_family, LPCSTR face);
HBITMAP WINAPI CreateDIBSection(HDC hdc, const BITMAPINFO *bmi, UINT usage, void **bits, HANDLE section, DWORD offset);
HDC WINAPI CreateCompatibleDC(HDC hdc);
HGDIOBJ WINAPI SelectObject(HDC hdc, HGDIOBJ obj);
COLORREF WINAPI SetBkColor(HDC hdc, COLORREF color);
COLORREF WINAPI SetTextColor(HDC hdc, COLORREF color);
BOOL WINAPI GetTextMetricsW(HDC hdc, LPTEXTMETRICW metrics);
BOOL WINAPI DeleteObject(HGDIOBJ obj);
BOOL WINAPI DeleteDC(HDC hdc);
BOOL WINAPI GetWindowRect(HWND hwnd, LPRECT rect);

#define CreateFont CreateFontA
#define GetTextMetrics GetTextMetricsW

#endif
