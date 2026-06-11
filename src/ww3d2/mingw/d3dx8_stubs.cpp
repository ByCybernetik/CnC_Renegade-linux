/*
** In-process D3DX8 replacements (no d3dx8.dll). Uses SDK headers from third_party/dxsdk8.
**
** D3D8 LockRect(pBits) points at the top-left of the locked RECT; pixel addresses
** are relative to that origin (0 .. width-1), not surface coordinates.
*/

#include <d3dx8.h>
#include "dx8wrapper.h"
#include <stdio.h>
#include <string.h>

extern "C" {

static void d3dx_full_rect(UINT width, UINT height, RECT &rect)
{
	rect.left = 0;
	rect.top = 0;
	rect.right = (LONG)width;
	rect.bottom = (LONG)height;
}

static void d3dx_resolve_rect(const RECT *rect, UINT width, UINT height, RECT &out)
{
	if (rect) {
		out = *rect;
	} else {
		d3dx_full_rect(width, height, out);
	}
}

static bool d3dx_is_32bit_format(D3DFORMAT format)
{
	return format == D3DFMT_A8R8G8B8
		|| format == D3DFMT_X8R8G8B8
		|| format == D3DFMT_R8G8B8;
}

static const unsigned char *d3dx_row_ptr(const RenegadeD3DLockedRect &locked, UINT row)
{
	return (const unsigned char *)Renegade_D3DLockedRect_PBits(locked) + row * locked.Pitch;
}

static unsigned char *d3dx_row_ptr_mut(RenegadeD3DLockedRect &locked, UINT row)
{
	return (unsigned char *)Renegade_D3DLockedRect_PBits(locked) + row * locked.Pitch;
}

static unsigned d3dx_read_pixel_at(const RenegadeD3DLockedRect &locked, UINT x, UINT y)
{
	const unsigned char *p =
		(const unsigned char *)Renegade_D3DLockedRect_PBits(locked) + y * locked.Pitch + x * 4;
	return *(const unsigned *)p;
}

static void d3dx_write_pixel_at(RenegadeD3DLockedRect &locked, UINT x, UINT y, unsigned color)
{
	unsigned char *p = (unsigned char *)Renegade_D3DLockedRect_PBits(locked) + y * locked.Pitch + x * 4;
	*(unsigned *)p = color;
}

static void d3dx_copy_same_size(
	const RenegadeD3DLockedRect &src_locked,
	UINT width,
	UINT height,
	RenegadeD3DLockedRect &dest_locked)
{
	const UINT row_bytes = width * 4;

	for (UINT y = 0; y < height; ++y) {
		memcpy(
			d3dx_row_ptr_mut(dest_locked, y),
			d3dx_row_ptr(src_locked, y),
			row_bytes);
	}
}

static void d3dx_box_downsample_2x(
	const RenegadeD3DLockedRect &src_locked,
	UINT src_w,
	UINT src_h,
	RenegadeD3DLockedRect &dest_locked,
	UINT dest_w,
	UINT dest_h)
{
	for (UINT dy = 0; dy < dest_h; ++dy) {
		const UINT sy0 = dy * 2;
		const UINT sy1 = sy0 + 1 < src_h ? sy0 + 1 : sy0;

		for (UINT dx = 0; dx < dest_w; ++dx) {
			const UINT sx0 = dx * 2;
			const UINT sx1 = sx0 + 1 < src_w ? sx0 + 1 : sx0;
			const unsigned c0 = d3dx_read_pixel_at(src_locked, sx0, sy0);
			const unsigned c1 = d3dx_read_pixel_at(src_locked, sx1, sy0);
			const unsigned c2 = d3dx_read_pixel_at(src_locked, sx0, sy1);
			const unsigned c3 = d3dx_read_pixel_at(src_locked, sx1, sy1);
			const unsigned a = ((c0 >> 24) + (c1 >> 24) + (c2 >> 24) + (c3 >> 24)) / 4;
			const unsigned r = (((c0 >> 16) & 0xff) + ((c1 >> 16) & 0xff)
				+ ((c2 >> 16) & 0xff) + ((c3 >> 16) & 0xff)) / 4;
			const unsigned g = (((c0 >> 8) & 0xff) + ((c1 >> 8) & 0xff)
				+ ((c2 >> 8) & 0xff) + ((c3 >> 8) & 0xff)) / 4;
			const unsigned b = ((c0 & 0xff) + (c1 & 0xff) + (c2 & 0xff) + (c3 & 0xff)) / 4;
			d3dx_write_pixel_at(
				dest_locked,
				dx,
				dy,
				(a << 24) | (r << 16) | (g << 8) | b);
		}
	}
}

static unsigned d3dx_lerp_color(float t, unsigned a, unsigned b)
{
	const float ar = (float)((a >> 16) & 0xff);
	const float ag = (float)((a >> 8) & 0xff);
	const float ab = (float)(a & 0xff);
	const float aa = (float)(a >> 24);
	const float br = (float)((b >> 16) & 0xff);
	const float bg = (float)((b >> 8) & 0xff);
	const float bb = (float)(b & 0xff);
	const float ba = (float)(b >> 24);
	const unsigned r = (unsigned)(ar + (br - ar) * t);
	const unsigned g = (unsigned)(ag + (bg - ag) * t);
	const unsigned bch = (unsigned)(ab + (bb - ab) * t);
	const unsigned al = (unsigned)(aa + (ba - aa) * t);
	return (al << 24) | (r << 16) | (g << 8) | bch;
}

static unsigned d3dx_sample_bilinear(
	const RenegadeD3DLockedRect &locked,
	UINT width,
	UINT height,
	float u,
	float v)
{
	const float x = u * (float)(width > 1 ? width - 1 : 0);
	const float y = v * (float)(height > 1 ? height - 1 : 0);
	int x0 = (int)x;
	int y0 = (int)y;
	int x1 = x0 + 1;
	int y1 = y0 + 1;
	if (x1 >= (int)width) {
		x1 = (int)width - 1;
	}
	if (y1 >= (int)height) {
		y1 = (int)height - 1;
	}
	const float fx = x - (float)x0;
	const float fy = y - (float)y0;

	const unsigned c00 = d3dx_read_pixel_at(locked, (UINT)x0, (UINT)y0);
	const unsigned c10 = d3dx_read_pixel_at(locked, (UINT)x1, (UINT)y0);
	const unsigned c01 = d3dx_read_pixel_at(locked, (UINT)x0, (UINT)y1);
	const unsigned c11 = d3dx_read_pixel_at(locked, (UINT)x1, (UINT)y1);

	const unsigned c0 = d3dx_lerp_color(fx, c00, c10);
	const unsigned c1 = d3dx_lerp_color(fx, c01, c11);
	return d3dx_lerp_color(fy, c0, c1);
}

static void d3dx_scale_surface(
	const RenegadeD3DLockedRect &src_locked,
	UINT src_w,
	UINT src_h,
	RenegadeD3DLockedRect &dest_locked,
	UINT dest_w,
	UINT dest_h,
	DWORD filter)
{
	const bool bilinear = (filter & (D3DX_FILTER_BOX | D3DX_FILTER_TRIANGLE)) != 0;

	for (UINT dy = 0; dy < dest_h; ++dy) {
		const float v = (dest_h > 1) ? (float)dy / (float)(dest_h - 1) : 0.0f;

		for (UINT dx = 0; dx < dest_w; ++dx) {
			const float u = (dest_w > 1) ? (float)dx / (float)(dest_w - 1) : 0.0f;
			unsigned color;
			if (bilinear) {
				color = d3dx_sample_bilinear(src_locked, src_w, src_h, u, v);
			} else {
				const UINT sx = (src_w > 1)
					? (UINT)((float)(src_w - 1) * u + 0.5f) : 0;
				const UINT sy = (src_h > 1)
					? (UINT)((float)(src_h - 1) * v + 0.5f) : 0;
				color = d3dx_read_pixel_at(src_locked, sx, sy);
			}
			d3dx_write_pixel_at(dest_locked, dx, dy, color);
		}
	}
}

HRESULT WINAPI D3DXGetErrorStringA(HRESULT hr, LPSTR buffer, UINT buffer_count)
{
	if (buffer && buffer_count > 0) {
		const char *msg = "D3D/D3DX error";
		strncpy(buffer, msg, buffer_count - 1);
		buffer[buffer_count - 1] = '\0';
	}
	return hr;
}

UINT WINAPI D3DXGetFVFVertexSize(DWORD fvf)
{
	UINT size = 0;
	if (fvf & D3DFVF_XYZ) size += 12;
	if (fvf & D3DFVF_NORMAL) size += 12;
	if (fvf & D3DFVF_DIFFUSE) size += 4;
	if (fvf & D3DFVF_SPECULAR) size += 4;
	UINT tc = (fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
	size += tc * 8;
	return size ? size : 32;
}

HRESULT WINAPI D3DXCreateTexture(
	LPDIRECT3DDEVICE8 device,
	UINT width,
	UINT height,
	UINT mip_levels,
	DWORD usage,
	D3DFORMAT format,
	D3DPOOL pool,
	LPDIRECT3DTEXTURE8 *texture)
{
	if (!device || !texture) {
		return D3DERR_INVALIDCALL;
	}
	*texture = NULL;
	return device->CreateTexture(
		width, height, mip_levels, usage, format, pool, texture);
}

HRESULT WINAPI D3DXCreateTextureFromFileExA(
	LPDIRECT3DDEVICE8, LPCSTR, UINT, UINT, UINT, DWORD, D3DFORMAT,
	D3DPOOL, DWORD, DWORD, D3DCOLOR, D3DXIMAGE_INFO *, PALETTEENTRY *,
	LPDIRECT3DTEXTURE8 *texture)
{
	if (texture) {
		*texture = NULL;
	}
	return E_FAIL;
}

HRESULT WINAPI D3DXLoadSurfaceFromSurface(
	LPDIRECT3DSURFACE8 dest,
	CONST PALETTEENTRY *dest_palette,
	CONST RECT *dest_rect,
	LPDIRECT3DSURFACE8 src,
	CONST PALETTEENTRY *src_palette,
	CONST RECT *src_rect,
	DWORD filter,
	D3DCOLOR color_key)
{
	(void)dest_palette;
	(void)src_palette;
	(void)color_key;

	if (!dest || !src) {
		return D3DERR_INVALIDCALL;
	}

	D3DSURFACE_DESC dest_desc;
	D3DSURFACE_DESC src_desc;
	if (FAILED(dest->GetDesc(&dest_desc)) || FAILED(src->GetDesc(&src_desc))) {
		return E_FAIL;
	}
	if (dest_desc.Format != src_desc.Format || !d3dx_is_32bit_format(dest_desc.Format)) {
		return E_FAIL;
	}

	RECT dest_r;
	RECT src_r;
	d3dx_resolve_rect(dest_rect, dest_desc.Width, dest_desc.Height, dest_r);
	d3dx_resolve_rect(src_rect, src_desc.Width, src_desc.Height, src_r);

	const UINT dest_w = (UINT)(dest_r.right - dest_r.left);
	const UINT dest_h = (UINT)(dest_r.bottom - dest_r.top);
	const UINT src_w = (UINT)(src_r.right - src_r.left);
	const UINT src_h = (UINT)(src_r.bottom - src_r.top);

	if (dest_w == 0 || dest_h == 0 || src_w == 0 || src_h == 0) {
		return D3D_OK;
	}


	RenegadeD3DLockedRect dest_locked;
	RenegadeD3DLockedRect src_locked;
	if (FAILED(dest->LockRect(Renegade_AsD3DLockedRect(&dest_locked), &dest_r, 0))
		|| Renegade_D3DLockedRect_PBits(dest_locked) == NULL) {
		return E_FAIL;
	}
	if (FAILED(src->LockRect(Renegade_AsD3DLockedRect(&src_locked), &src_r, D3DLOCK_READONLY))
		|| Renegade_D3DLockedRect_PBits(src_locked) == NULL) {
		dest->UnlockRect();
		return E_FAIL;
	}

	if (dest_w == src_w && dest_h == src_h) {
		IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
		if (device) {
			POINT dest_point;
			dest_point.x = dest_r.left;
			dest_point.y = dest_r.top;
			const HRESULT copy_hr = device->CopyRects(
				src,
				&src_r,
				1,
				dest,
				&dest_point);
			if (SUCCEEDED(copy_hr)) {
				src->UnlockRect();
				dest->UnlockRect();
				return D3D_OK;
			}
		}
		d3dx_copy_same_size(src_locked, src_w, src_h, dest_locked);
	} else if (dest_w * 2 == src_w && dest_h * 2 == src_h && (filter & D3DX_FILTER_BOX)) {
		d3dx_box_downsample_2x(src_locked, src_w, src_h, dest_locked, dest_w, dest_h);
	} else {
		d3dx_scale_surface(src_locked, src_w, src_h, dest_locked, dest_w, dest_h, filter);
	}

	src->UnlockRect();
	dest->UnlockRect();
	return D3D_OK;
}

HRESULT WINAPI D3DXFilterTexture(
	LPDIRECT3DBASETEXTURE8 base_texture,
	CONST PALETTEENTRY *palette,
	UINT src_level,
	DWORD filter)
{
	if (!base_texture) {
		return D3DERR_INVALIDCALL;
	}

	LPDIRECT3DTEXTURE8 texture = (LPDIRECT3DTEXTURE8)base_texture;
	const UINT level_count = texture->GetLevelCount();
	for (UINT level = src_level + 1; level < level_count; ++level) {
		LPDIRECT3DSURFACE8 src_surface = NULL;
		LPDIRECT3DSURFACE8 dest_surface = NULL;
		if (FAILED(texture->GetSurfaceLevel(level - 1, &src_surface))) {
			return E_FAIL;
		}
		if (FAILED(texture->GetSurfaceLevel(level, &dest_surface))) {
			src_surface->Release();
			return E_FAIL;
		}

		const HRESULT hr = D3DXLoadSurfaceFromSurface(
			dest_surface,
			palette,
			NULL,
			src_surface,
			palette,
			NULL,
			filter,
			0);

		src_surface->Release();
		dest_surface->Release();
		if (FAILED(hr)) {
			return hr;
		}
	}

	return D3D_OK;
}

}
