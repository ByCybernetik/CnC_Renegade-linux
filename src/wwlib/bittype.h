/*
**	Command & Conquer Renegade(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* $Header: /G/wwlib/bittype.h 4     4/02/99 1:37p Eric_c $ */
/*************************************************************************** 
 ***                  Confidential - Westwood Studios                    *** 
 *************************************************************************** 
 *                                                                         * 
 *                 Project Name : Voxel Technology                         * 
 *                                                                         * 
 *                    File Name : BITTYPE.H                                * 
 *                                                                         * 
 *                   Programmer : Greg Hjelstrom                           * 
 *                                                                         * 
 *                   Start Date : 02/24/97                                 * 
 *                                                                         * 
 *                  Last Update : February 24, 1997 [GH]                   * 
 *                                                                         * 
 *-------------------------------------------------------------------------* 
 * Functions:                                                              * 
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef BITTYPE_H
#define BITTYPE_H

#if defined(RENEGADE_LINUX)
#include <stdint.h>
typedef uint8_t		uint8;
typedef uint16_t	uint16;
typedef uint32_t	uint32;
typedef int8_t		sint8;
typedef int16_t		sint16;
typedef int32_t		sint32;
#else
typedef unsigned char	uint8;
typedef unsigned short	uint16;
typedef unsigned long	uint32;
typedef signed char		sint8;
typedef signed short		sint16;
typedef signed long		sint32;
#endif
typedef unsigned int    uint;
typedef signed int      sint;

typedef float				float32;
typedef double				float64;

#if defined(RENEGADE_LINUX)
#include <windows.h>
/* Win32 ULONG/DWORD are 32-bit; Linux <windows.h> uses unsigned long (64-bit on LP64). */
#undef ULONG
#undef DWORD
typedef uint32_t ULONG;
typedef uint32_t DWORD;
#else
typedef unsigned long   DWORD;
typedef unsigned short	WORD;
typedef unsigned char   BYTE;
typedef int             BOOL;
typedef unsigned short	USHORT;
typedef const char *		LPCSTR;
typedef unsigned int    UINT;
typedef unsigned long   ULONG;
#endif

#endif //BITTYPE_H
