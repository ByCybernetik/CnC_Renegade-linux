#ifndef RENEGADE_DLGS_H
#define RENEGADE_DLGS_H

#include <windows.h>

/* Win32 dialog templates are packed (18-byte headers/items); default GCC alignment is 20. */
#pragma pack(push, 1)

typedef struct tagDLGTEMPLATE {
	DWORD style;
	DWORD dwExtendedStyle;
	WORD cdit;
	short x;
	short y;
	short cx;
	short cy;
} DLGTEMPLATE;

typedef DLGTEMPLATE *LPDLGTEMPLATE;

typedef struct tagDLGITEMTEMPLATE {
	DWORD style;
	DWORD dwExtendedStyle;
	short x;
	short y;
	short cx;
	short cy;
	WORD id;
} DLGITEMTEMPLATE;

typedef DLGITEMTEMPLATE *LPDLGITEMTEMPLATE;

#pragma pack(pop)

#ifndef DS_SETFONT
#define DS_SETFONT 0x0040L
#endif
#ifndef DS_MODALFRAME
#define DS_MODALFRAME 0x0080L
#endif
#ifndef DS_SETFOREGROUND
#define DS_SETFOREGROUND 0x0200L
#endif

#endif
