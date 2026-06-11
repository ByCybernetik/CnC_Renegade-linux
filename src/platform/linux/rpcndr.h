#ifndef RENEGADE_RPCNDR_H
#define RENEGADE_RPCNDR_H

#ifndef __RPCNDR_H_VERSION__
#define __RPCNDR_H_VERSION__ 501
#endif

#include <rpc.h>

#ifdef __cplusplus
extern "C" {
#endif

void *__RPC_USER MIDL_user_allocate(size_t size);
void __RPC_USER MIDL_user_free(void *ptr);

typedef struct _RPC_MESSAGE *PRPC_MESSAGE;

#ifndef CONST_VTBL
#define CONST_VTBL
#endif

#define MIDL_INTERFACE(x) struct

#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C extern
#endif

#ifdef __cplusplus
}
#endif

#endif
