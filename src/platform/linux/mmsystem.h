#ifndef RENEGADE_MMSYSTEM_H
#define RENEGADE_MMSYSTEM_H

#if defined(RENEGADE_WW3D2_BUILD)
#include <windows.h>
#else
#include "renegade_win32_shim.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif
typedef UINT MMRESULT;
#ifndef TIMERR_NOERROR
#define TIMERR_NOERROR 0
#endif

DWORD timeGetTime(void);
MMRESULT timeBeginPeriod(UINT period);
MMRESULT timeEndPeriod(UINT period);
#ifdef __cplusplus
}
#endif

#endif
