/*
**	Command & Conquer Renegade(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	GCC / MinGW compatibility layer (mirrors visualc.h for MSVC).
*/

#ifndef GCC_H
#define GCC_H

#if defined(__GNUC__)

/* C++98 and later have built-in bool; bool.h is only for legacy C / old MSVC. */
#if !defined(__cplusplus)
#include "bool.h"
#endif

#ifndef M_PI
#define M_PI        3.14159265358979323846
#define M_LOG2E     1.44269504088896340736
#define M_LOG10E    0.434294481903251827651
#define M_LN2       0.693147180559945309417
#define M_LN10      2.30258509299404568402
#define M_PI_2      1.57079632679489661923
#define M_PI_4      0.785398163397448309616
#define M_1_PI      0.318309886183790671538
#define M_2_PI      0.636619772367581343076
#define M_SQRT2     1.41421356237309504880
#define M_SQRT_2    0.707106781186547524401
#endif

#ifndef __forceinline
#define __forceinline inline __attribute__((always_inline))
#endif

#endif /* __GNUC__ */

#endif /* GCC_H */
