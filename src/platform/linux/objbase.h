#ifndef RENEGADE_OBJBASE_H
#define RENEGADE_OBJBASE_H

#include <windows.h>
#include <unknwn.h>
#include <objidl.h>

#ifndef LPUNKNOWN
typedef IUnknown *LPUNKNOWN;
#endif

typedef GUID CLSID;

#ifndef CLSCTX_INPROC_SERVER
#define CLSCTX_INPROC_SERVER 0x1
#endif
#ifndef STDMETHODIMP
#define STDMETHODIMP HRESULT
#endif

HRESULT CoCreateInstance(
	REFCLSID rclsid,
	LPUNKNOWN pUnkOuter,
	DWORD dwClsContext,
	REFIID riid,
	LPVOID *ppv);

HRESULT CoInitialize(LPVOID reserved);
void CoUninitialize(void);

#ifndef DECLSPEC_UUID
#ifdef __cplusplus
#define DECLSPEC_UUID(s) __attribute__((uuid(s)))
#else
#define DECLSPEC_UUID(s)
#endif
#endif

#endif
