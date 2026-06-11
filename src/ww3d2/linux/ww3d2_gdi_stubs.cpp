#include "ww3d2_gdi_stubs.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

/* Supersample glyphs before downsampling to reduce jagged edges in UI text. */
static const int FONT_OVERSAMPLE = 2;


enum GdiObjectKind {
	GDI_KIND_FONT = 0x464F4E54,   /* 'FONT' */
	GDI_KIND_BITMAP = 0x424D5041, /* 'BMPA' */
	GDI_KIND_DC = 0x4D454D44,     /* 'MEMD' */
};

struct CachedFontFace {
	char name[64];
	unsigned char *ttf_buffer;
	stbtt_fontinfo stb_font;
	bool ready;
};

struct WW3D2_GdiFontState {
	CachedFontFace *face;
	int point_size;
	int char_w;
	int char_h;
	float scale;
	int ascent;
	int descent;
	bool bold;
};

struct GdiFontObject {
	GdiObjectKind kind;
	WW3D2_GdiFontState state;
};

struct GdiBitmapObject {
	GdiObjectKind kind;
	uint8_t *bits;
	size_t bits_size;
	int w;
	int h;
	int stride;
};

struct GdiMemDc {
	GdiObjectKind kind;
	WW3D2_GdiFontState font_state;
	GdiBitmapObject *bitmap;
};

static CachedFontFace g_face_cache[8];
static int g_face_cache_count = 0;
static bool g_logged_first_font = false;

static bool read_file_into_buffer(const char *path, unsigned char **out, size_t *out_len)
{
	FILE *f = fopen(path, "rb");
	if (!f) {
		return false;
	}
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return false;
	}
	const long sz = ftell(f);
	if (sz <= 0 || sz > 32 * 1024 * 1024) {
		fclose(f);
		return false;
	}
	rewind(f);
	unsigned char *buf = (unsigned char *)malloc((size_t)sz);
	if (!buf) {
		fclose(f);
		return false;
	}
	if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
		free(buf);
		fclose(f);
		return false;
	}
	fclose(f);
	*out = buf;
	*out_len = (size_t)sz;
	return true;
}

static void wide_face_to_ascii(LPCWSTR face, char *out, size_t out_len)
{
	if (!out || out_len == 0) {
		return;
	}
	out[0] = '\0';
	if (!face) {
		return;
	}
	size_t i = 0;
	for (; i + 1 < out_len && face[i] != L'\0'; i++) {
		out[i] = (face[i] < 128) ? (char)face[i] : '?';
	}
	out[i] = '\0';
}

static void normalize_face_key(const char *face, char *key, size_t key_len)
{
	if (!key || key_len == 0) {
		return;
	}
	key[0] = '\0';
	if (!face || face[0] == '\0') {
		std::strncpy(key, "default", key_len - 1);
		key[key_len - 1] = '\0';
		return;
	}

	if (std::strstr(face, "Regatta") != NULL) {
		std::strncpy(key, "Regatta Condensed LET", key_len - 1);
	} else if (std::strstr(face, "Arial") != NULL || std::strstr(face, "MT") != NULL) {
		std::strncpy(key, "Arial MT", key_len - 1);
	} else if (std::strstr(face, "Microsoft Sans") != NULL) {
		std::strncpy(key, "Microsoft Sans Serif", key_len - 1);
	} else {
		std::strncpy(key, face, key_len - 1);
	}
	key[key_len - 1] = '\0';
}

static bool try_load_face_from_paths(CachedFontFace *face, const char *const *paths)
{
	for (int i = 0; paths[i] != NULL; i++) {
		unsigned char *buf = NULL;
		size_t len = 0;
		if (!read_file_into_buffer(paths[i], &buf, &len)) {
			continue;
		}
		stbtt_fontinfo info;
		if (!stbtt_InitFont(&info, buf, stbtt_GetFontOffsetForIndex(buf, 0))) {
			free(buf);
			continue;
		}
		if (face->ttf_buffer) {
			free(face->ttf_buffer);
		}
		face->ttf_buffer = buf;
		face->stb_font = info;
		face->ready = true;

		return true;
	}
	return false;
}

static CachedFontFace *find_or_load_face(const char *face_name)
{
	char key[64];
	normalize_face_key(face_name, key, sizeof(key));

	for (int i = 0; i < g_face_cache_count; i++) {
		if (std::strcmp(g_face_cache[i].name, key) == 0) {
			return &g_face_cache[i];
		}
	}

	if (g_face_cache_count >= (int)(sizeof(g_face_cache) / sizeof(g_face_cache[0]))) {
		return &g_face_cache[0];
	}

	CachedFontFace *face = &g_face_cache[g_face_cache_count++];
	std::memset(face, 0, sizeof(*face));
	std::strncpy(face->name, key, sizeof(face->name) - 1);

	static const char *const game_and_system_paths[] = {
		"ARI_____.TTF",
		"ARI_____.ttf",
		"54251___.TTF",
		"54251___.ttf",
		"micross.ttf",
		"Data/ARI_____.TTF",
		"Data/ARI_____.ttf",
		"Data/54251___.TTF",
		"Data/54251___.ttf",
		"Data/micross.ttf",
		"arial.ttf",
		"ARIAL.TTF",
		"data/arial.ttf",
		"Data/arial.ttf",
		"/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
		"/usr/share/fonts/TTF/LiberationSans-Regular.ttf",
		"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
		"/usr/share/fonts/TTF/DejaVuSans.ttf",
		"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
		"/usr/share/fonts/noto/NotoSans-Regular.ttf",
		NULL,
	};

	static const char *const regatta_paths[] = {
		"54251___.TTF",
		"54251___.ttf",
		"Data/54251___.TTF",
		"Data/54251___.ttf",
		"ARI_____.TTF",
		"ARI_____.ttf",
		"/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
		"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
		NULL,
	};

	static const char *const arial_paths[] = {
		"ARI_____.TTF",
		"ARI_____.ttf",
		"Data/ARI_____.TTF",
		"Data/ARI_____.ttf",
		"arial.ttf",
		"ARIAL.TTF",
		"/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
		"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
		NULL,
	};

	static const char *const micross_paths[] = {
		"micross.ttf",
		"Data/micross.ttf",
		"ARI_____.TTF",
		"/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
		NULL,
	};

	const char *const *paths = game_and_system_paths;
	if (std::strcmp(key, "Regatta Condensed LET") == 0) {
		paths = regatta_paths;
	} else if (std::strcmp(key, "Arial MT") == 0) {
		paths = arial_paths;
	} else if (std::strcmp(key, "Microsoft Sans Serif") == 0) {
		paths = micross_paths;
	}

	if (!try_load_face_from_paths(face, paths)) {
		try_load_face_from_paths(face, game_and_system_paths);
	}

	return face;
}

static void update_font_metrics(WW3D2_GdiFontState *st)
{
	if (!st) {
		return;
	}

	if (st->face == NULL) {
		st->face = find_or_load_face("Arial MT");
	}

	if (!st->face || !st->face->ready) {
		st->scale = 1.0f;
		st->char_w = st->point_size / 2 + 2;
		st->char_h = st->point_size;
		st->ascent = st->point_size - 2;
		st->descent = 2;
		return;
	}

	st->scale = stbtt_ScaleForPixelHeight(&st->face->stb_font, (float)st->point_size);
	int ascent = 0;
	int descent = 0;
	int line_gap = 0;
	stbtt_GetFontVMetrics(&st->face->stb_font, &ascent, &descent, &line_gap);
	st->ascent = (int)(ascent * st->scale + 0.5f);
	st->descent = (int)(-descent * st->scale + 0.5f);
	st->char_h = st->ascent + st->descent;
	if (st->char_h < 1) {
		st->char_h = st->point_size;
	}
	st->char_w = st->point_size / 2 + 2;
}

static GdiMemDc *mem_dc_from_hdc(HDC hdc)
{
	if (hdc == NULL || hdc == (HDC)1) {
		return NULL;
	}
	GdiMemDc *dc = reinterpret_cast<GdiMemDc *>(hdc);
	if (dc->kind != GDI_KIND_DC) {
		return NULL;
	}
	return dc;
}

static GdiFontObject *font_from_handle(HGDIOBJ obj)
{
	if (obj == NULL || obj == (HGDIOBJ)1) {
		return NULL;
	}
	GdiFontObject *font = reinterpret_cast<GdiFontObject *>(obj);
	if (font->kind != GDI_KIND_FONT) {
		return NULL;
	}
	return font;
}

static GdiBitmapObject *bitmap_from_handle(HGDIOBJ obj)
{
	if (obj == NULL || obj == (HGDIOBJ)1) {
		return NULL;
	}
	GdiBitmapObject *bitmap = reinterpret_cast<GdiBitmapObject *>(obj);
	if (bitmap->kind != GDI_KIND_BITMAP) {
		return NULL;
	}
	return bitmap;
}

static int codepoint_from_wchar(WCHAR ch)
{
	return (int)(unsigned short)ch;
}

static void glyph_pixel_metrics(
	WW3D2_GdiFontState *st,
	int cp,
	int *advance_px,
	int *bbox_w)
{
	if (advance_px) {
		*advance_px = st ? st->char_w : 8;
	}
	if (bbox_w) {
		*bbox_w = 0;
	}

	if (!st || !st->face || !st->face->ready) {
		return;
	}

	int advance = 0;
	int lsb = 0;
	stbtt_GetCodepointHMetrics(&st->face->stb_font, cp, &advance, &lsb);

	float scale = st->scale;
	if (st->bold) {
		scale *= 1.04f;
	}

	if (advance_px) {
		*advance_px = (int)(advance * scale + 0.5f);
		if (st->bold && *advance_px > 0) {
			(*advance_px)++;
		}
		if (*advance_px < 1) {
			*advance_px = 1;
		}
	}

	if (bbox_w) {
		int x0 = 0;
		int y0 = 0;
		int x1 = 0;
		int y1 = 0;
		stbtt_GetCodepointBitmapBox(&st->face->stb_font, cp, scale, scale, &x0, &y0, &x1, &y1);
		*bbox_w = x1 - x0;
	}
}

static void measure_codepoint(WW3D2_GdiFontState *st, int cp, LPSIZE size)
{
	if (!size) {
		return;
	}

	int advance_px = 8;
	int bbox_w = 0;
	glyph_pixel_metrics(st, cp, &advance_px, &bbox_w);

	size->cx = advance_px;
	if (bbox_w > size->cx) {
		size->cx = bbox_w;
	}
	size->cy = st ? st->char_h : 12;
}

static void write_alpha_pixel(GdiBitmapObject *bitmap, int px, int py, uint8_t alpha)
{
	if (!bitmap || !bitmap->bits || alpha == 0) {
		return;
	}
	if (px < 0 || py < 0 || px >= bitmap->w || py >= bitmap->h) {
		return;
	}

	const size_t idx = (size_t)(py * bitmap->stride + px * 3);
	if (idx + 2 < bitmap->bits_size) {
		const uint8_t existing = bitmap->bits[idx];
		if (alpha > existing) {
			bitmap->bits[idx] = alpha;
			bitmap->bits[idx + 1] = alpha;
			bitmap->bits[idx + 2] = alpha;
		}
	}
}

static void blit_glyph_downsampled(
	GdiBitmapObject *bitmap,
	WW3D2_GdiFontState *st,
	int x,
	int y,
	unsigned char *glyph,
	int gw,
	int gh,
	int xoff,
	int yoff)
{
	if (!bitmap || !bitmap->bits || !st || !glyph || gw <= 0 || gh <= 0) {
		return;
	}

	const int baseline_y = y + st->ascent;
	const int os = FONT_OVERSAMPLE;
	const int out_w = (gw + os - 1) / os;
	const int out_h = (gh + os - 1) / os;

	for (int out_row = 0; out_row < out_h; out_row++) {
		for (int out_col = 0; out_col < out_w; out_col++) {
			int sum = 0;
			int count = 0;
			for (int sy = 0; sy < os; sy++) {
				for (int sx = 0; sx < os; sx++) {
					const int src_row = out_row * os + sy;
					const int src_col = out_col * os + sx;
					if (src_row < gh && src_col < gw) {
						sum += glyph[src_row * gw + src_col];
						count++;
					}
				}
			}

			if (count <= 0) {
				continue;
			}

			const uint8_t alpha = (uint8_t)((sum + count / 2) / count);
			const int px = x + xoff + out_col;
			const int py = baseline_y + yoff + out_row;
			write_alpha_pixel(bitmap, px, py, alpha);
		}
	}
}

static void draw_codepoint_to_dib(
	GdiBitmapObject *bitmap,
	WW3D2_GdiFontState *st,
	int x,
	int y,
	int cp,
	const RECT *rect)
{
	if (!bitmap || !bitmap->bits || !st) {
		return;
	}

	if (rect && (rect->right > rect->left) && (rect->bottom > rect->top)) {
		for (int row = rect->top; row < rect->bottom && row < bitmap->h; row++) {
			for (int col = rect->left; col < rect->right && col < bitmap->w; col++) {
				const size_t idx = (size_t)(row * bitmap->stride + col * 3);
				if (idx + 2 < bitmap->bits_size) {
					bitmap->bits[idx] = 0;
					bitmap->bits[idx + 1] = 0;
					bitmap->bits[idx + 2] = 0;
				}
			}
		}
	}

	if (!st->face || !st->face->ready) {
		const int w = st->char_w;
		const int h = st->char_h;
		for (int row = y; row < y + h && row < bitmap->h; row++) {
			for (int col = x; col < x + w && col < bitmap->w; col++) {
				const size_t idx = (size_t)(row * bitmap->stride + col * 3);
				if (idx + 2 < bitmap->bits_size) {
					bitmap->bits[idx] = bitmap->bits[idx + 1] = bitmap->bits[idx + 2] = 255;
				}
			}
		}
		return;
	}

	int gw = 0;
	int gh = 0;
	int xoff = 0;
	int yoff = 0;
	float render_scale = st->scale * (float)FONT_OVERSAMPLE;
	if (st->bold) {
		render_scale *= 1.04f;
	}

	unsigned char *glyph = stbtt_GetCodepointBitmap(
		&st->face->stb_font,
		render_scale,
		render_scale,
		cp,
		&gw,
		&gh,
		&xoff,
		&yoff);
	if (!glyph) {
		return;
	}

	xoff = (xoff + FONT_OVERSAMPLE / 2) / FONT_OVERSAMPLE;
	yoff = (yoff + FONT_OVERSAMPLE / 2) / FONT_OVERSAMPLE;
	blit_glyph_downsampled(bitmap, st, x, y, glyph, gw, gh, xoff, yoff);
	stbtt_FreeBitmap(glyph, NULL);
}

HDC WINAPI GetDC(HWND)
{
	return (HDC)1;
}

int WINAPI ReleaseDC(HWND, HDC)
{
	return 1;
}

BOOL WINAPI GetTextExtentPoint32W(HDC hdc, LPCWSTR str, int count, LPSIZE size)
{
	GdiMemDc *dc = mem_dc_from_hdc(hdc);
	if (!size) {
		return FALSE;
	}
	const int cp = (str != NULL && count > 0) ? codepoint_from_wchar(str[0]) : (int)L'?';
	measure_codepoint(dc ? &dc->font_state : NULL, cp, size);
	return TRUE;
}

BOOL WINAPI ExtTextOutW(
	HDC hdc,
	int x,
	int y,
	UINT options,
	const RECT *rect,
	LPCWSTR str,
	UINT count,
	const INT *)
{
	GdiMemDc *dc = mem_dc_from_hdc(hdc);
	if (!dc || !dc->bitmap || !dc->bitmap->bits) {
		return TRUE;
	}
	const int cp =
		(str != NULL && count > 0) ? codepoint_from_wchar(str[0]) : (int)L'?';
	if (options & ETO_OPAQUE) {
		draw_codepoint_to_dib(dc->bitmap, &dc->font_state, x, y, cp, rect);
	} else {
		draw_codepoint_to_dib(dc->bitmap, &dc->font_state, x, y, cp, NULL);
	}
	return TRUE;
}

int WINAPI MulDiv(int n, int numerator, int denominator)
{
	if (denominator == 0) {
		return 0;
	}
	return (int)((long long)n * numerator / denominator);
}

int WINAPI GetDeviceCaps(HDC, int index)
{
	return index == LOGPIXELSY ? 96 : 0;
}

UINT WINAPI GetACP(void)
{
	return 1252;
}

HFONT WINAPI CreateFontW(
	int height,
	int,
	int,
	int,
	int weight,
	DWORD,
	DWORD,
	DWORD,
	DWORD,
	DWORD,
	DWORD,
	DWORD,
	DWORD,
	LPCWSTR face)
{
	GdiFontObject *font = new GdiFontObject();
	std::memset(font, 0, sizeof(*font));
	font->kind = GDI_KIND_FONT;

	font->state.point_size = height < 0 ? -height : height;
	if (font->state.point_size <= 0) {
		font->state.point_size = 12;
	}
	font->state.bold = (weight >= FW_BOLD);

	char face_ascii[64];
	wide_face_to_ascii(face, face_ascii, sizeof(face_ascii));
	font->state.face = find_or_load_face(face_ascii);
	update_font_metrics(&font->state);
	return (HFONT)font;
}

HFONT WINAPI CreateFontA(
	int height,
	int w,
	int esc,
	int orient,
	int weight,
	DWORD italic,
	DWORD underline,
	DWORD strikeout,
	DWORD charset,
	DWORD out_prec,
	DWORD clip_prec,
	DWORD quality,
	DWORD pitch,
	LPCSTR face)
{
	(void)w;
	(void)esc;
	(void)orient;
	(void)italic;
	(void)underline;
	(void)strikeout;
	(void)charset;
	(void)out_prec;
	(void)clip_prec;
	(void)quality;
	(void)pitch;

	WCHAR wface[64] = { 0 };
	if (face != NULL) {
		for (int i = 0; i < 63 && face[i] != '\0'; i++) {
			wface[i] = (WCHAR)(unsigned char)face[i];
		}
	}

	return CreateFontW(
		height,
		0,
		0,
		0,
		weight,
		italic,
		underline,
		strikeout,
		charset,
		out_prec,
		clip_prec,
		quality,
		pitch,
		wface);
}

HBITMAP WINAPI CreateDIBSection(HDC, const BITMAPINFO *bmi, UINT, void **bits, HANDLE, DWORD)
{
	GdiBitmapObject *bitmap = new GdiBitmapObject();
	std::memset(bitmap, 0, sizeof(*bitmap));
	bitmap->kind = GDI_KIND_BITMAP;

	bitmap->w = bmi ? (int)bmi->bmiHeader.biWidth : 32;
	bitmap->h = bmi ? abs((int)bmi->bmiHeader.biHeight) : 32;
	bitmap->stride = ((bitmap->w * 3 + 3) & ~3);
	bitmap->bits_size = (size_t)(bitmap->stride * bitmap->h);
	bitmap->bits = (uint8_t *)std::malloc(bitmap->bits_size);
	if (bitmap->bits) {
		std::memset(bitmap->bits, 0, bitmap->bits_size);
	}
	if (bits) {
		*bits = bitmap->bits;
	}
	return (HBITMAP)bitmap;
}

HDC WINAPI CreateCompatibleDC(HDC)
{
	GdiMemDc *dc = new GdiMemDc();
	std::memset(dc, 0, sizeof(*dc));
	dc->kind = GDI_KIND_DC;
	dc->font_state.point_size = 12;
	dc->font_state.face = find_or_load_face("Arial MT");
	update_font_metrics(&dc->font_state);
	return (HDC)dc;
}

HGDIOBJ WINAPI SelectObject(HDC hdc, HGDIOBJ obj)
{
	GdiMemDc *dc = mem_dc_from_hdc(hdc);
	if (!dc) {
		return NULL;
	}

	GdiFontObject *font = font_from_handle(obj);
	if (font != NULL) {
		dc->font_state = font->state;
		return obj;
	}

	GdiBitmapObject *bitmap = bitmap_from_handle(obj);
	if (bitmap != NULL) {
		dc->bitmap = bitmap;
		return obj;
	}

	return NULL;
}

COLORREF WINAPI SetBkColor(HDC, COLORREF)
{
	return 0;
}

COLORREF WINAPI SetTextColor(HDC, COLORREF)
{
	return 0xFFFFFF;
}

BOOL WINAPI GetTextMetricsW(HDC hdc, LPTEXTMETRICW metrics)
{
	GdiMemDc *dc = mem_dc_from_hdc(hdc);
	if (!metrics) {
		return FALSE;
	}
	std::memset(metrics, 0, sizeof(*metrics));
	metrics->tmHeight = dc ? dc->font_state.char_h : 12;
	metrics->tmAscent = dc ? dc->font_state.ascent : (metrics->tmHeight - 2);
	return TRUE;
}

BOOL WINAPI DeleteObject(HGDIOBJ obj)
{
	if (obj == NULL || obj == (HGDIOBJ)1) {
		return TRUE;
	}

	GdiFontObject *font = font_from_handle(obj);
	if (font != NULL) {
		delete font;
		return TRUE;
	}

	GdiBitmapObject *bitmap = bitmap_from_handle(obj);
	if (bitmap != NULL) {
		std::free(bitmap->bits);
		delete bitmap;
		return TRUE;
	}

	return TRUE;
}

BOOL WINAPI GetWindowRect(HWND, LPRECT rect)
{
	if (rect) {
		rect->left = 0;
		rect->top = 0;
		rect->right = 800;
		rect->bottom = 600;
	}
	return TRUE;
}

BOOL WINAPI DeleteDC(HDC hdc)
{
	GdiMemDc *dc = mem_dc_from_hdc(hdc);
	if (dc != NULL) {
		delete dc;
	}
	return TRUE;
}
