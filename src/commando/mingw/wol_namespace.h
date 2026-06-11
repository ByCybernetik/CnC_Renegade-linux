/*
 * MinGW: WOLAPI at global scope; WOL::Type expands to ::Type via empty WOL macro.
 * Avoids nesting wolapi in namespace WOL (breaks ocidl.h / system COM).
 */
#ifndef COMMANDO_MINGW_WOL_NAMESPACE_H_
#define COMMANDO_MINGW_WOL_NAMESPACE_H_

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <ole2.h>
#include <WOLAPI/wolapi.h>
#include <WOLAPI/ChatDefs.h>

#if defined(__MINGW32__) || defined(__MINGW64__) || defined(RENEGADE_LINUX)
#define WOL
#endif

#endif
