#ifndef RENEGADE_RPC_H
#define RENEGADE_RPC_H

#include <windows.h>

typedef GUID CLSID;

#ifndef DECLSPEC_UUID
#ifdef __cplusplus
#define DECLSPEC_UUID(s) __attribute__((uuid(s)))
#else
#define DECLSPEC_UUID(s)
#endif
#endif

#ifndef __OBJC__
#undef interface
#define interface struct
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define __RPC_FAR
#define __RPC_API
#define __RPC_USER __RPC_API
#define __RPC_STUB __RPC_API
#define RPC_ENTRY __RPC_API

#ifdef __cplusplus
}
#endif

#endif
