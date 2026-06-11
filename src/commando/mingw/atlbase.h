/*
 * Minimal ATL stubs for MinGW cross-build (WOL COM code).
 */
#ifndef COMMANDO_MINGW_ATLBASE_H_
#define COMMANDO_MINGW_ATLBASE_H_

#include <unknwn.h>
#include <ocidl.h>
#ifdef RENEGADE_LINUX
#include "objbase.h"
#endif

static inline HRESULT Mingw_AdviseConnectionPoint(
    IUnknown* punk, IUnknown* sink, const IID& iid, DWORD* cookie)
{
    if (!punk || !sink || !cookie) {
        return E_POINTER;
    }
    IConnectionPointContainer* container = NULL;
    HRESULT hr = punk->QueryInterface(
        IID_IConnectionPointContainer, (void**)&container);
    if (FAILED(hr)) {
        return hr;
    }
    IConnectionPoint* cp = NULL;
    hr = container->FindConnectionPoint(iid, &cp);
    container->Release();
    if (FAILED(hr)) {
        return hr;
    }
    hr = cp->Advise(sink, cookie);
    cp->Release();
    return hr;
}

static inline HRESULT Mingw_UnadviseConnectionPoint(
    IUnknown* punk, const IID& iid, DWORD cookie)
{
    if (!punk) {
        return E_POINTER;
    }
    IConnectionPointContainer* container = NULL;
    HRESULT hr = punk->QueryInterface(
        IID_IConnectionPointContainer, (void**)&container);
    if (FAILED(hr)) {
        return hr;
    }
    IConnectionPoint* cp = NULL;
    hr = container->FindConnectionPoint(iid, &cp);
    container->Release();
    if (FAILED(hr)) {
        return hr;
    }
    hr = cp->Unadvise(cookie);
    cp->Release();
    return hr;
}

template <class T>
class CComPtr {
public:
    CComPtr() : p_(NULL) {}
    CComPtr(T* p) : p_(p) { if (p_) p_->AddRef(); }
    CComPtr(const CComPtr& o) : p_(o.p_) { if (p_) p_->AddRef(); }
    ~CComPtr() { if (p_) p_->Release(); }

    void Attach(T* p) {
        if (p_) p_->Release();
        p_ = p;
    }

    T* Detach() {
        T* p = p_;
        p_ = NULL;
        return p;
    }

    T* operator->() const { return p_; }
    operator T*() const { return p_; }
    T** operator&() { return &p_; }

    bool operator!() const { return p_ == NULL; }
    int operator==(int zero) const { return zero == 0 ? (p_ == NULL) : 0; }
    int operator!=(int zero) const { return zero == 0 ? (p_ != NULL) : 1; }

    CComPtr& operator=(T* p) {
        if (p != p_) {
            if (p_) p_->Release();
            p_ = p;
            if (p_) p_->AddRef();
        }
        return *this;
    }

    CComPtr& operator=(const CComPtr& o) {
        return operator=(o.p_);
    }

    HRESULT Advise(IUnknown* sink, const IID& iid, DWORD* cookie) {
        return Mingw_AdviseConnectionPoint(p_, sink, iid, cookie);
    }

#if defined(RENEGADE_LINUX)
    HRESULT Advise(IUnknown* sink, const IID& iid, unsigned long* cookie) {
        return Mingw_AdviseConnectionPoint(p_, sink, iid,
            reinterpret_cast<DWORD*>(cookie));
    }

    template <class Sink>
    HRESULT Advise(Sink* sink, const IID& iid, unsigned long* cookie) {
        return Mingw_AdviseConnectionPoint(p_, reinterpret_cast<IUnknown*>(sink),
            iid, reinterpret_cast<DWORD*>(cookie));
    }

    template <class Sink>
    HRESULT Advise(const CComPtr<Sink>& sink, const IID& iid, unsigned long* cookie) {
        return Mingw_AdviseConnectionPoint(p_, sink, iid,
            reinterpret_cast<DWORD*>(cookie));
    }
#endif

    void Release() {
        if (p_) {
            p_->Release();
            p_ = NULL;
        }
    }

private:
    T* p_;
};

template <class T>
HRESULT AtlAdvise(T* obj, IUnknown* sink, const IID& iid, DWORD* cookie) {
    return Mingw_AdviseConnectionPoint(obj, sink, iid, cookie);
}

template <class T>
HRESULT AtlAdvise(CComPtr<T>& obj, IUnknown* sink, const IID& iid, DWORD* cookie) {
    return Mingw_AdviseConnectionPoint(obj, sink, iid, cookie);
}

template <class T, class Sink>
HRESULT AtlAdvise(CComPtr<T>& obj, Sink* sink, const IID& iid, DWORD* cookie) {
    return Mingw_AdviseConnectionPoint(obj, reinterpret_cast<IUnknown*>(sink), iid, cookie);
}

#if defined(RENEGADE_LINUX)
template <class T, class Sink>
HRESULT AtlAdvise(CComPtr<T>& obj, Sink* sink, const IID& iid, unsigned long* cookie) {
    return Mingw_AdviseConnectionPoint(obj, reinterpret_cast<IUnknown*>(sink), iid,
        reinterpret_cast<DWORD*>(cookie));
}
#endif

template <class T>
HRESULT AtlUnadvise(T* obj, const IID& iid, DWORD cookie) {
    return Mingw_UnadviseConnectionPoint(obj, iid, cookie);
}

template <class T>
HRESULT AtlUnadvise(CComPtr<T>& obj, const IID& iid, DWORD cookie) {
    return Mingw_UnadviseConnectionPoint(obj, iid, cookie);
}

#if defined(RENEGADE_LINUX)
template <class T, class Sink>
HRESULT AtlAdvise(const CComPtr<T>& obj, const CComPtr<Sink>& sink, const IID& iid,
    unsigned long* cookie) {
    return Mingw_AdviseConnectionPoint(obj, static_cast<IUnknown*>(sink),
        iid, reinterpret_cast<DWORD*>(cookie));
}
#endif

#endif
