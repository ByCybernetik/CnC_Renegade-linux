#ifndef RENEGADE_PROCESS_H
#define RENEGADE_PROCESS_H
#include <stdint.h>
#include <pthread.h>
#include "renegade_win32_shim.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned (__stdcall *ThreadFn)(void *);

unsigned long _beginthread(ThreadFn start, unsigned stack, void *arg);
uintptr_t _beginthreadex(void *security, unsigned stack_size, ThreadFn start_address,
	void *arglist, unsigned initflag, unsigned *thrdaddr);

#ifdef __cplusplus
}
#endif

#endif
