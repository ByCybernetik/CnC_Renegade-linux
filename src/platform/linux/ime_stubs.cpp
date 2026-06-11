#include "imm.h"
#include <cstring>

static HIMC g_ime_ctx = (HIMC)1;
static HIMC g_default_ctx = (HIMC)2;

UINT ImmGetVirtualKey(HWND) { return 0; }

HIMC ImmGetContext(HWND) { return g_ime_ctx; }

BOOL ImmReleaseContext(HWND, HIMC) { return TRUE; }

HIMC ImmCreateContext(void) { return g_ime_ctx; }

BOOL ImmDestroyContext(HIMC) { return TRUE; }

HIMC ImmAssociateContext(HWND, HIMC imc) { return imc ? imc : g_default_ctx; }

BOOL ImmGetOpenStatus(HIMC) { return FALSE; }

BOOL ImmSetOpenStatus(HIMC, BOOL) { return TRUE; }

BOOL ImmGetConversionStatus(HIMC, LPDWORD conv, LPDWORD sent)
{
	if (conv) {
		*conv = 0;
	}
	if (sent) {
		*sent = 0;
	}
	return TRUE;
}

BOOL ImmSetConversionStatus(HIMC, DWORD, DWORD) { return TRUE; }

DWORD ImmGetProperty(HKL, DWORD) { return 0; }

UINT ImmGetDescriptionW(HKL, LPWSTR buf, UINT buflen)
{
	if (buf && buflen > 0) {
		buf[0] = 0;
	}
	return 0;
}

UINT ImmGetDescription(HKL, LPSTR buf, UINT buflen)
{
	if (buf && buflen > 0) {
		buf[0] = 0;
	}
	return 0;
}

LONG ImmGetCompositionStringW(HIMC, DWORD, LPVOID, DWORD) { return 0; }

LONG ImmGetCompositionString(HIMC, DWORD, LPVOID, DWORD) { return 0; }

BOOL ImmGetCompositionFont(HIMC, LPVOID) { return FALSE; }

DWORD ImmGetGuideLine(HIMC, DWORD, LPVOID, DWORD) { return 0; }

DWORD ImmGetGuideLineW(HIMC, DWORD, LPWSTR, DWORD) { return 0; }

static DWORD candidate_list_size(void)
{
	return (DWORD)sizeof(CANDIDATELIST);
}

DWORD ImmGetCandidateListW(HIMC, DWORD, LPCANDIDATELIST list, DWORD size)
{
	const DWORD need = candidate_list_size();
	if (!list || size < need) {
		return need;
	}
	std::memset(list, 0, need);
	list->dwSize = need;
	return need;
}

DWORD ImmGetCandidateList(HIMC imc, DWORD index, LPCANDIDATELIST list, DWORD size)
{
	return ImmGetCandidateListW(imc, index, list, size);
}

BOOL ImmNotifyIME(HIMC, DWORD, DWORD, DWORD) { return TRUE; }

BOOL ImmSetCompositionWindow(HIMC, LPCOMPOSITIONFORM) { return TRUE; }

BOOL ImmSetCandidateWindow(HIMC, LPCANDIDATEFORM) { return TRUE; }
