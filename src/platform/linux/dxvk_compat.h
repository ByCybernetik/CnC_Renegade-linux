/* Force-included when compiling ww3d2 (D3DX8 types on top of dxvk windows.h). */
#ifndef RENEGADE_DXVK_COMPAT_H
#define RENEGADE_DXVK_COMPAT_H

#if defined(RENEGADE_LINUX) && (defined(RENEGADE_WW3D2_BUILD) || defined(RENEGADE_USE_D3DX8))

#include <windows.h>
#include <unknwn.h>
#include <stdlib.h>

#ifndef LPUNKNOWN
typedef IUnknown *LPUNKNOWN;
#endif

#ifndef FAR
#define FAR
#endif
#ifndef NEAR
#define NEAR
#endif
#ifndef OPTIONAL
#define OPTIONAL
#endif
#ifndef IN
#define IN
#endif
#ifndef OUT
#define OUT
#endif

#ifndef _MAX_PATH
#define _MAX_PATH 260
#endif
#ifndef _MAX_FNAME
#define _MAX_FNAME 256
#endif
#ifndef _MAX_EXT
#define _MAX_EXT 256
#endif

#ifndef LPBYTE
typedef BYTE *LPBYTE;
typedef BYTE *PBYTE;
#endif
#ifndef LPCTSTR
typedef LPCSTR LPCTSTR;
#endif
#ifndef LPGUID
typedef GUID *LPGUID;
#endif
#ifndef STDAPI
#define STDAPI HRESULT WINAPI
#define STDAPI_(type) type WINAPI
#endif

typedef HANDLE HFONT;

#ifndef tagLOGFONTA
typedef struct tagLOGFONTA {
	LONG lfHeight;
	LONG lfWidth;
	LONG lfEscapement;
	LONG lfOrientation;
	LONG lfWeight;
	BYTE lfItalic;
	BYTE lfUnderline;
	BYTE lfStrikeOut;
	BYTE lfCharSet;
	BYTE lfOutPrecision;
	BYTE lfClipPrecision;
	BYTE lfQuality;
	BYTE lfPitchAndFamily;
	CHAR lfFaceName[32];
} LOGFONTA, *LPLOGFONTA;
#endif
#ifndef LOGFONT
#define LOGFONT LOGFONTA
#endif
#ifndef LPLOGFONT
#define LPLOGFONT LPLOGFONTA
#endif

struct IStream;

typedef HANDLE HBITMAP;

#ifndef _GLYPHMETRICSFLOAT_DEFINED
#define _GLYPHMETRICSFLOAT_DEFINED
typedef struct _GLYPHMETRICSFLOAT {
	FLOAT gmfBlackBoxX;
	FLOAT gmfBlackBoxY;
	struct {
		FLOAT x;
		FLOAT y;
	} gmfptGlyphOrigin;
	FLOAT gmfCellIncX;
	FLOAT gmfCellIncY;
} GLYPHMETRICSFLOAT, *LPGLYPHMETRICSFLOAT;
#endif

#include "d3d8_lockrect.h"

#endif /* RENEGADE_LINUX && RENEGADE_WW3D2_BUILD */

#endif /* RENEGADE_DXVK_COMPAT_H */
