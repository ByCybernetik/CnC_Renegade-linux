#ifndef RENEGADE_WWAUDIO_PROCESS_H
#define RENEGADE_WWAUDIO_PROCESS_H

/* Stub for Windows <process.h>. Declares _beginthread for Linux builds. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uintptr_t _beginthread(void (*proc)(void*), unsigned stack_size, void* arg);

#ifdef __cplusplus
}
#endif

#endif /* RENEGADE_WWAUDIO_PROCESS_H */
