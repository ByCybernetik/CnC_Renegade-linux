#ifndef RENEGADE_D3D8_LOCKRECT_H
#define RENEGADE_D3D8_LOCKRECT_H

#include <d3d8.h>

/*
 * DXSDK8 d3d8types.h uses #pragma pack(4): D3DLOCKED_RECT is 12 bytes with pBits at +4.
 * dxvk-native writes 16 bytes with pBits at +8 on 64-bit. Use RenegadeD3DLockedRect for LockRect.
 */
#if defined(RENEGADE_LINUX) && (defined(__x86_64__) || defined(__aarch64__) || defined(_LP64))

struct RenegadeD3DLockedRect {
	INT Pitch;
	INT Reserved;
	void *pBits;
};

inline D3DLOCKED_RECT *Renegade_AsD3DLockedRect(RenegadeD3DLockedRect *locked_rect)
{
	return reinterpret_cast<D3DLOCKED_RECT *>(locked_rect);
}

inline const D3DLOCKED_RECT *Renegade_AsD3DLockedRect(const RenegadeD3DLockedRect *locked_rect)
{
	return reinterpret_cast<const D3DLOCKED_RECT *>(locked_rect);
}

inline void *Renegade_D3DLockedRect_PBits(const RenegadeD3DLockedRect &locked_rect)
{
	return locked_rect.pBits;
}

#else

typedef D3DLOCKED_RECT RenegadeD3DLockedRect;

inline D3DLOCKED_RECT *Renegade_AsD3DLockedRect(RenegadeD3DLockedRect *locked_rect)
{
	return locked_rect;
}

inline const D3DLOCKED_RECT *Renegade_AsD3DLockedRect(const RenegadeD3DLockedRect *locked_rect)
{
	return locked_rect;
}

inline void *Renegade_D3DLockedRect_PBits(const RenegadeD3DLockedRect &locked_rect)
{
	return locked_rect.pBits;
}

#endif

#endif /* RENEGADE_D3D8_LOCKRECT_H */
