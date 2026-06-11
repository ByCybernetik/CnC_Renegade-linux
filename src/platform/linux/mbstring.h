#ifndef RENEGADE_MBSTRING_H
#define RENEGADE_MBSTRING_H

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline unsigned char *_mbsinc(unsigned char *s)
{
	return s ? s + 1 : s;
}

static inline size_t _mbslen(unsigned char *s)
{
	return s ? strlen((const char *)s) : 0;
}

static inline size_t _mbsnccnt(unsigned char *s, size_t n)
{
	(void)s;
	return n;
}

#ifdef __cplusplus
}
#endif

#endif
