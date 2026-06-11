#ifndef RENEGADE_OLE2_H
#define RENEGADE_OLE2_H

#include <windows.h>
#include <unknwn.h>
#include <objidl.h>

typedef GUID CLSID;

#ifndef DECLSPEC_UUID
#ifdef __cplusplus
#define DECLSPEC_UUID(s) __attribute__((uuid(s)))
#else
#define DECLSPEC_UUID(s)
#endif
#endif

#endif
