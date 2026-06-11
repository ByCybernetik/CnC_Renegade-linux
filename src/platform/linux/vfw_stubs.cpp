#include "vfw.h"
#include <stdlib.h>
#include <string.h>

HRESULT AVIFileInit(void) { return 0; }
HRESULT AVIFileOpenA(PAVIFILE *ppfile, LPCSTR, DWORD, void *)
{
	if (ppfile) {
		*ppfile = NULL;
	}
	return 1;
}
HRESULT AVIFileCreateStream(PAVIFILE, PAVISTREAM *ppstream, AVISTREAMINFO *)
{
	if (ppstream) {
		*ppstream = NULL;
	}
	return 1;
}
HRESULT AVIStreamSetFormat(PAVISTREAM, LONG, void *, LONG) { return 0; }
HRESULT AVIStreamWrite(PAVISTREAM, LONG, LONG, void *, LONG, DWORD, LONG *, LONG *) { return 0; }
HRESULT AVIFileRelease(PAVIFILE) { return 0; }
HRESULT AVIStreamRelease(PAVISTREAM) { return 0; }
void AVIFileExit(void) {}
DWORD mmioFOURCC(char a, char b, char c, char d)
{
	return (DWORD)(unsigned char)a | ((DWORD)(unsigned char)b << 8) |
		((DWORD)(unsigned char)c << 16) | ((DWORD)(unsigned char)d << 24);
}
void *GlobalAllocPtr(unsigned, size_t bytes) { return malloc(bytes); }
void GlobalFreePtr(void *ptr) { free(ptr); }
