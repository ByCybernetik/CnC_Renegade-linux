#ifndef RENEGADE_VFW_H
#define RENEGADE_VFW_H
#include <stddef.h>
#include <windows.h>
#if defined(RENEGADE_WW3D2_BUILD)
#include "ww3d2_win32_extra.h"
#else
#include "renegade_win32_shim.h"
#endif

typedef void *PAVIFILE;
typedef void *PAVISTREAM;

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
} BITMAPINFOHEADER;

typedef struct {
	DWORD fccType;
	DWORD fccHandler;
	DWORD dwFlags;
	DWORD dwCaps;
	WORD wPriority;
	WORD wLanguage;
	DWORD dwScale;
	DWORD dwRate;
	DWORD dwStart;
	DWORD dwLength;
	DWORD dwInitialFrames;
	DWORD dwSuggestedBufferSize;
	DWORD dwQuality;
	DWORD dwSampleSize;
	RECT rcFrame;
	DWORD dwEditCount;
	DWORD dwFormatChangeCount;
	char szName[64];
} AVISTREAMINFO;

#define BI_RGB 0
#define streamtypeVIDEO 0x73646976u
#define OF_WRITE 0x0001
#define OF_CREATE 0x1000
#define GMEM_MOVEABLE 0x0002

#ifdef __cplusplus
extern "C" {
#endif

HRESULT AVIFileInit(void);
HRESULT AVIFileOpenA(PAVIFILE *ppfile, LPCSTR name, DWORD mode, void *p);
HRESULT AVIFileCreateStream(PAVIFILE pfile, PAVISTREAM *ppstream, AVISTREAMINFO *psi);
HRESULT AVIStreamSetFormat(PAVISTREAM pstream, LONG pos, void *format, LONG size);
HRESULT AVIStreamWrite(PAVISTREAM pstream, LONG start, LONG samples, void *buffer, LONG size, DWORD flags,
	LONG *sample, LONG *count);
HRESULT AVIFileRelease(PAVIFILE pfile);
HRESULT AVIStreamRelease(PAVISTREAM pstream);
void AVIFileExit(void);
DWORD mmioFOURCC(char a, char b, char c, char d);
void *GlobalAllocPtr(unsigned flags, size_t bytes);
void GlobalFreePtr(void *ptr);

#ifdef __cplusplus
}
#endif

#endif
