#ifndef RENEGADE_IMAGEHLP_H
#define RENEGADE_IMAGEHLP_H

#include <windows.h>

/* Stack-walk / symbol stubs (debug.cpp forward decls; bodies are #if 0). */
typedef struct _CONTEXT {
	DWORD ContextFlags;
	DWORD Eip;
	DWORD Esp;
	DWORD Ebp;
} CONTEXT, *PCONTEXT;

typedef struct _EXCEPTION_RECORD {
	DWORD ExceptionCode;
	DWORD ExceptionFlags;
	void *ExceptionRecord;
	void *ExceptionAddress;
	DWORD NumberParameters;
	ULONG ExceptionInformation[15];
} EXCEPTION_RECORD, *PEXCEPTION_RECORD;

typedef struct _EXCEPTION_POINTERS {
	EXCEPTION_RECORD *ExceptionRecord;
	CONTEXT *ContextRecord;
} EXCEPTION_POINTERS, *PEXCEPTION_POINTERS;

typedef LONG (WINAPI *PTOP_LEVEL_EXCEPTION_FILTER)(PEXCEPTION_POINTERS);
typedef PTOP_LEVEL_EXCEPTION_FILTER LPTOP_LEVEL_EXCEPTION_FILTER;

#ifdef __cplusplus
extern "C" {
#endif

BOOL SymInitialize(HANDLE proc, LPCSTR path, BOOL invade);
BOOL SymCleanup(HANDLE proc);

#ifdef __cplusplus
}
#endif

#endif
