/*
 * pthread-backed Win32 events and joinable thread handles for Linux bring-up.
 */
#define _GNU_SOURCE
#include "linux_sync.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static const uint32_t RENEGADE_HANDLE_MAGIC = 0x52454E48u; /* 'RENH' */

enum RenegadeHandleType {
	RENEGADE_HANDLE_EVENT = 1,
	RENEGADE_HANDLE_THREAD = 2,
};

struct RenegadeEvent {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	bool signaled;
	bool manual_reset;
};

struct RenegadeThread {
	pthread_t thread;
	volatile bool exited;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
};

struct RenegadeHandle {
	uint32_t magic;
	RenegadeHandleType type;
	union {
		RenegadeEvent *event;
		RenegadeThread *thread;
	} u;
};

static RenegadeHandle *handle_from_ptr(HANDLE h)
{
	if (h == NULL || h == INVALID_HANDLE_VALUE) {
		return NULL;
	}

	/*
	 * Legacy stubs returned small integer "handles" (e.g. CreateMutex -> 3).
	 * Never dereference those — they are not mapped memory.
	 */
	const uintptr_t addr = (uintptr_t)h;
	if (addr < 4096u) {
		return NULL;
	}

	RenegadeHandle *handle = (RenegadeHandle *)h;
	if (handle->magic != RENEGADE_HANDLE_MAGIC) {
		return NULL;
	}
	return handle;
}

BOOL renegade_sync_is_typed_handle(HANDLE h)
{
	return handle_from_ptr(h) != NULL;
}

static void timespec_add_ms(struct timespec *ts, DWORD ms)
{
	ts->tv_sec += (time_t)(ms / 1000u);
	ts->tv_nsec += (long)((ms % 1000u) * 1000000L);
	if (ts->tv_nsec >= 1000000000L) {
		ts->tv_sec += 1;
		ts->tv_nsec -= 1000000000L;
	}
}

static bool thread_join_timeout(pthread_t tid, DWORD ms)
{
	if (ms == INFINITE) {
		return pthread_join(tid, NULL) == 0;
	}

	struct timespec deadline;
	clock_gettime(CLOCK_REALTIME, &deadline);
	timespec_add_ms(&deadline, ms);

	for (;;) {
		void *ret = NULL;
#if defined(__GLIBC__) && defined(_GNU_SOURCE)
		if (pthread_tryjoin_np(tid, &ret) == 0) {
			return true;
		}
#else
		if (pthread_join(tid, &ret) == 0) {
			return true;
		}
#endif
		struct timespec now;
		clock_gettime(CLOCK_REALTIME, &now);
		if (now.tv_sec > deadline.tv_sec ||
			(now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec)) {
			return false;
		}
		usleep(1000);
	}
}

HANDLE renegade_sync_create_event(BOOL manual_reset, BOOL initial_state)
{
	RenegadeHandle *handle = (RenegadeHandle *)calloc(1, sizeof(RenegadeHandle));
	RenegadeEvent *event = (RenegadeEvent *)calloc(1, sizeof(RenegadeEvent));
	if (!handle || !event) {
		free(handle);
		free(event);
		return NULL;
	}

	pthread_mutex_init(&event->mutex, NULL);
	pthread_cond_init(&event->cond, NULL);
	event->signaled = initial_state != FALSE;
	event->manual_reset = manual_reset != FALSE;

	handle->magic = RENEGADE_HANDLE_MAGIC;
	handle->type = RENEGADE_HANDLE_EVENT;
	handle->u.event = event;
	return (HANDLE)handle;
}

BOOL renegade_sync_set_event(HANDLE h)
{
	RenegadeHandle *handle = handle_from_ptr(h);
	if (!handle || handle->type != RENEGADE_HANDLE_EVENT) {
		return FALSE;
	}

	RenegadeEvent *event = handle->u.event;
	pthread_mutex_lock(&event->mutex);
	event->signaled = true;
	pthread_cond_broadcast(&event->cond);
	pthread_mutex_unlock(&event->mutex);
	return TRUE;
}

BOOL renegade_sync_reset_event(HANDLE h)
{
	RenegadeHandle *handle = handle_from_ptr(h);
	if (!handle || handle->type != RENEGADE_HANDLE_EVENT) {
		return FALSE;
	}

	RenegadeEvent *event = handle->u.event;
	pthread_mutex_lock(&event->mutex);
	event->signaled = false;
	pthread_mutex_unlock(&event->mutex);
	return TRUE;
}

DWORD renegade_sync_wait_for_single_object(HANDLE h, DWORD ms)
{
	RenegadeHandle *handle = handle_from_ptr(h);
	if (!handle) {
		return WAIT_FAILED;
	}

	if (handle->type == RENEGADE_HANDLE_EVENT) {
		RenegadeEvent *event = handle->u.event;
		pthread_mutex_lock(&event->mutex);

		if (!event->signaled) {
			if (ms == 0) {
				pthread_mutex_unlock(&event->mutex);
				return WAIT_TIMEOUT;
			}

			if (ms == INFINITE) {
				while (!event->signaled) {
					pthread_cond_wait(&event->cond, &event->mutex);
				}
			} else {
				struct timespec deadline;
				clock_gettime(CLOCK_REALTIME, &deadline);
				timespec_add_ms(&deadline, ms);
				while (!event->signaled) {
					const int rc = pthread_cond_timedwait(
						&event->cond,
						&event->mutex,
						&deadline);
					if (rc == ETIMEDOUT) {
						pthread_mutex_unlock(&event->mutex);
						return WAIT_TIMEOUT;
					}
				}
			}
		}

		if (!event->manual_reset) {
			event->signaled = false;
		}
		pthread_mutex_unlock(&event->mutex);
		return WAIT_OBJECT_0;
	}

	if (handle->type == RENEGADE_HANDLE_THREAD) {
		RenegadeThread *thread = handle->u.thread;
		if (thread_join_timeout(thread->thread, ms)) {
			return WAIT_OBJECT_0;
		}
		return WAIT_TIMEOUT;
	}

	return WAIT_FAILED;
}

BOOL renegade_sync_close_handle(HANDLE h)
{
	RenegadeHandle *handle = handle_from_ptr(h);
	if (!handle) {
		return FALSE;
	}

	if (handle->type == RENEGADE_HANDLE_EVENT) {
		RenegadeEvent *event = handle->u.event;
		pthread_mutex_destroy(&event->mutex);
		pthread_cond_destroy(&event->cond);
		free(event);
	} else if (handle->type == RENEGADE_HANDLE_THREAD) {
		RenegadeThread *thread = handle->u.thread;
		/* Avoid shutdown deadlock if the thread did not exit in time. */
		if (!thread_join_timeout(thread->thread, 3000)) {
			pthread_detach(thread->thread);
		}
		pthread_mutex_destroy(&thread->mutex);
		pthread_cond_destroy(&thread->cond);
		free(thread);
	}

	free(handle);
	return TRUE;
}

struct ThreadStartBundle {
	void (*start)(void *);
	void *arg;
};

static void *thread_trampoline_safe(void *param)
{
	ThreadStartBundle *bundle = (ThreadStartBundle *)param;
	void (*start)(void *) = bundle->start;
	void *arg = bundle->arg;
	free(bundle);
	start(arg);
	return NULL;
}

uintptr_t renegade_sync_begin_thread(void (*start)(void *), void *arg)
{
	RenegadeHandle *handle = (RenegadeHandle *)calloc(1, sizeof(RenegadeHandle));
	RenegadeThread *thread = (RenegadeThread *)calloc(1, sizeof(RenegadeThread));
	ThreadStartBundle *bundle =
		(ThreadStartBundle *)malloc(sizeof(ThreadStartBundle));
	if (!handle || !thread || !bundle) {
		free(handle);
		free(thread);
		free(bundle);
		return 0;
	}

	bundle->start = start;
	bundle->arg = arg;

	pthread_mutex_init(&thread->mutex, NULL);
	pthread_cond_init(&thread->cond, NULL);
	thread->exited = false;

	if (pthread_create(&thread->thread, NULL, thread_trampoline_safe, bundle) != 0) {
		pthread_mutex_destroy(&thread->mutex);
		pthread_cond_destroy(&thread->cond);
		free(handle);
		free(thread);
		free(bundle);
		return 0;
	}

	handle->magic = RENEGADE_HANDLE_MAGIC;
	handle->type = RENEGADE_HANDLE_THREAD;
	handle->u.thread = thread;
	return (uintptr_t)handle;
}
