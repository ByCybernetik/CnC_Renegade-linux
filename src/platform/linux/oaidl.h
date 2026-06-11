#ifndef RENEGADE_OAIDL_H
#define RENEGADE_OAIDL_H

#include <ole2.h>
#include "winuser_extra.h"

typedef LONG DISPID;
typedef WORD VARTYPE;
typedef WCHAR OLECHAR;
typedef OLECHAR *LPOLESTR;
typedef OLECHAR *BSTR;

typedef const void *RPC_IF_HANDLE;

struct ITypeInfo;
struct EXCEPINFO;

typedef struct tagVARIANT VARIANT;
typedef VARIANT VARIANTARG;

typedef struct tagDISPPARAMS {
	VARIANT *rgvarg;
	DISPID *rgdispidNamedArgs;
	UINT cArgs;
	UINT cNamedArgs;
} DISPPARAMS, *LPDISPPARAMS;

#ifdef __cplusplus

MIDL_INTERFACE("00020400-0000-0000-C000-000000000046")
IDispatch : public IUnknown {
public:
	virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT *pctinfo) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID riid, LPOLESTR *rgszNames, UINT cNames,
		LCID lcid, DISPID *rgDispId) = 0;
	virtual HRESULT STDMETHODCALLTYPE Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
		DISPPARAMS *pDispParams, VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr) = 0;
};

#else

typedef interface IDispatch IDispatch;

#endif

#endif
