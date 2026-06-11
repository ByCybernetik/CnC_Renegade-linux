#ifndef RENEGADE_WINBASE_H
#define RENEGADE_WINBASE_H

#include <windows.h>
#include <limits.h>
#include "win32_minimal.h"

#ifdef __cplusplus
extern "C" {
#endif

LONG InterlockedIncrement(LONG volatile *addend);
LONG InterlockedDecrement(LONG volatile *addend);

#ifdef __cplusplus
}
#endif

#endif
