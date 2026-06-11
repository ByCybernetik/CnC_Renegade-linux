#ifndef RENEGADE_LINUX_SYNC_H
#define RENEGADE_LINUX_SYNC_H

#include "renegade_win32_shim.h"

#ifdef __cplusplus
extern "C" {
#endif

BOOL renegade_sync_is_typed_handle(HANDLE h);
HANDLE renegade_sync_create_event(BOOL manual_reset, BOOL initial_state);
BOOL renegade_sync_set_event(HANDLE h);
BOOL renegade_sync_reset_event(HANDLE h);
DWORD renegade_sync_wait_for_single_object(HANDLE h, DWORD ms);
BOOL renegade_sync_close_handle(HANDLE h);

uintptr_t renegade_sync_begin_thread(void (*start)(void *), void *arg);

#ifdef __cplusplus
}
#endif

#endif
