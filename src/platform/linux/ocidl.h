#ifndef RENEGADE_OCIDL_H
#define RENEGADE_OCIDL_H

#include <unknwn.h>

DEFINE_GUID(IID_IConnectionPointContainer, 0xb196b203, 0xbab4, 0x101a, 0xb6, 0x9c, 0x00, 0xaa, 0x00, 0x34, 0x1d, 0x07);
DEFINE_GUID(IID_IConnectionPoint, 0xb196b201, 0xbab4, 0x101a, 0xb6, 0x9c, 0x00, 0xaa, 0x00, 0x34, 0x1d, 0x07);

#ifdef __cplusplus

struct IEnumConnectionPoints;
struct IEnumConnections;
struct IConnectionPointContainer;

MIDL_INTERFACE("B196B201-BAB4-101A-B69C-00AA00341D07")
IConnectionPoint : public IUnknown {
public:
	virtual HRESULT STDMETHODCALLTYPE GetConnectionInterface(IID *pIID) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetConnectionPointContainer(
		IConnectionPointContainer **ppCPC) = 0;
	virtual HRESULT STDMETHODCALLTYPE Advise(IUnknown *pUnkSink, DWORD *pdwCookie) = 0;
	virtual HRESULT STDMETHODCALLTYPE Unadvise(DWORD dwCookie) = 0;
	virtual HRESULT STDMETHODCALLTYPE EnumConnections(IEnumConnections **ppEnum) = 0;
};

MIDL_INTERFACE("B196B203-BAB4-101A-B69C-00AA00341D07")
IConnectionPointContainer : public IUnknown {
public:
	virtual HRESULT STDMETHODCALLTYPE EnumConnectionPoints(
		IEnumConnectionPoints **ppEnum) = 0;
	virtual HRESULT STDMETHODCALLTYPE FindConnectionPoint(
		REFIID riid, IConnectionPoint **ppCP) = 0;
};

#else

typedef interface IConnectionPointContainer IConnectionPointContainer;
typedef interface IConnectionPoint IConnectionPoint;

#endif

#endif
