#ifndef MILES_WIN32_UTIL_H
#define MILES_WIN32_UTIL_H

#include <windows.h>

static inline void *miles_get_proc(HMODULE mod, const char *plain, const char *decorated)
{
	void *fn;

	if (mod == NULL) {
		return NULL;
	}

	fn = (void *)GetProcAddress(mod, plain);
	if (fn != NULL) {
		return fn;
	}
	if (decorated != NULL) {
		fn = (void *)GetProcAddress(mod, decorated);
	}
	return fn;
}

#endif
