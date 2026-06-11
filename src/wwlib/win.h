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

/*********************************************************************************************** 
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               *** 
 *********************************************************************************************** 
 *                                                                                             * 
 *                 Project Name : Command & Conquer                                            * 
 *                                                                                             * 
 *                     $Archive:: /Commando/Code/wwlib/win.h                                  $* 
 *                                                                                             * 
 *                      $Author:: Ian_l                                                       $*
 *                                                                                             * 
 *                     $Modtime:: 10/16/01 2:42p                                              $*
 *                                                                                             * 
 *                    $Revision:: 11                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------* 
 * Functions:                                                                                  * 
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef WIN_H
#define WIN_H

/*
**	This header file includes the Windows headers. If there are any special pragmas that need
**	to occur around this process, they are performed here. Typically, certain warnings will need
**	to be disabled since the Windows headers are repleat with illegal and dangerous constructs.
**
**	Within the windows headers themselves, Microsoft has disabled the warnings 4290, 4514, 
**	4069, 4200, 4237, 4103, 4001, 4035, 4164. Makes you wonder, eh?
*/

// When including windows, lets just bump the warning level back to 3...
#if (_MSC_VER >= 1200)
#pragma warning(push, 3)
#endif

// this define should also be in the DSP just in case someone includes windows stuff directly
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#if defined(RENEGADE_LINUX)
#include <stdint.h>
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __stdcall
#define __stdcall
#endif
#include <windows.h>
#include "../platform/linux/winres.h"
#include "../platform/linux/dlgs.h"
#include "../platform/linux/winuser.h"
#if !defined(RENEGADE_WW3D2_BUILD)
#include "../platform/linux/wingdi.h"
#include "../platform/linux/wincon.h"
#include "../platform/linux/win32_minimal.h"
#include "../platform/linux/renegade_win32_shim.h"
#include "ww_wcstring.h"
#else
#include "../platform/linux/ww3d2_win32_extra.h"
#include "ww_wcstring.h"
#endif
#ifdef __cplusplus
static inline LONG WINAPI InterlockedCompareExchange(volatile LONG *dest, LONG exchange, LONG compare)
{
	return __sync_val_compare_and_swap(dest, compare, exchange);
}
#endif
#include "osdep.h"
#else
#include	<windows.h>
#endif
//#include <mmsystem.h>
//#include	<windowsx.h>
//#include	<winnt.h>
//#include	<winuser.h>

/* wingdi.h defines X/Y as macros; breaks TPoint3D::X / ::Y member names. */
#ifdef X
#undef X
#endif
#ifdef Y
#undef Y
#endif

#if (_MSC_VER >= 1200)
#pragma warning(pop)
#endif

#if defined(_WINDOWS) || defined(RENEGADE_LINUX)
extern HINSTANCE	ProgramInstance;
extern HWND			MainWindow;
extern bool GameInFocus;

#ifdef _DEBUG

void __cdecl Print_Win32Error(unsigned long win32Error);

#else // _DEBUG

#define Print_Win32Error

#endif // _DEBUG

#elif !defined(RENEGADE_LINUX)
//#include <unistd.h>
#endif

#endif // WIN_H
