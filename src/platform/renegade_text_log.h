#ifndef RENEGADE_TEXT_LOG_H
#define RENEGADE_TEXT_LOG_H

/*
 * Font / text pipeline tracing for Wine (MinGW), native Win32, and Linux.
 * Writes JSON lines to renegade_boot.log (requires RENEGADE_BOOT_LOG).
 *
 * Fields tagged with hypothesisId "T" (text). Compare runs across:
 *   - target: linux | win32
 *   - runtime: native | wine  (win32 build under Wine sets runtime=wine)
 *   - arch: x86_32 | x86_64
 *   - ptr / size_t / long / wchar sizes (LP32 vs LP64)
 */

#if defined(RENEGADE_BOOT_LOG)

#include "renegade_agent_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#if defined(RENEGADE_LINUX)
#include "linux/renegade_win32_shim.h"
#include "../wwlib/ww_wcstring.h"
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

// #region agent log
static inline const char *Renegade_Text_Target_Name(void)
{
#if defined(RENEGADE_LINUX)
	return "linux";
#else
	return "win32";
#endif
}

static inline const char *Renegade_Text_Arch_Name(void)
{
#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__) || defined(_M_ARM64)
	return "x86_64";
#elif defined(__i386__) || defined(_M_IX86) || defined(__i686__)
	return "x86_32";
#else
	return "unknown";
#endif
}

static inline const char *Renegade_Text_Runtime_Name(void)
{
#if defined(RENEGADE_LINUX)
	return "native";
#else
	if (getenv("WINEPREFIX") != NULL || getenv("WINELOADER") != NULL) {
		return "wine";
	}
	return "native";
#endif
}

static inline void Renegade_Text_Escape_Json(const char *src, char *dst, int dst_len)
{
	int j = 0;
	if (dst == NULL || dst_len <= 0) {
		return;
	}
	if (src == NULL) {
		dst[0] = '\0';
		return;
	}
	for (int i = 0; src[i] != '\0' && j < dst_len - 2; i++) {
		const unsigned char c = (unsigned char)src[i];
		if (c == '"' || c == '\\') {
			dst[j++] = '_';
		} else if (c >= 32u && c < 127u) {
			dst[j++] = (char)c;
		} else {
			dst[j++] = '?';
		}
	}
	dst[j] = '\0';
}

static inline void Renegade_Text_Wide_Preview(const WCHAR *text, char *preview, int preview_len)
{
	if (preview == NULL || preview_len <= 0) {
		return;
	}
	preview[0] = '\0';
	if (text == NULL) {
		return;
	}
	char buffer[128];
	buffer[0] = '\0';
	::WideCharToMultiByte(
		0,
		0,
		text,
		-1,
		buffer,
		(int)sizeof(buffer) - 1,
		NULL,
		NULL);
	Renegade_Text_Escape_Json(buffer, preview, preview_len);
}

static inline int Renegade_Text_Wide_Len(const WCHAR *text)
{
	return text != NULL ? (int)wcslen(text) : 0;
}

static inline void Renegade_Text_Log_Platform(void)
{
	static int logged = 0;
	if (logged) {
		return;
	}
	logged = 1;

	char json[384];
	snprintf(
		json,
		sizeof(json),
		"{\"target\":\"%s\",\"runtime\":\"%s\",\"arch\":\"%s\","
		"\"lp_bits\":%u,\"ptr\":%u,\"size_t\":%u,\"long\":%u,"
		"\"wchar\":%u,\"int\":%u,\"short_wchar\":%d}",
		Renegade_Text_Target_Name(),
		Renegade_Text_Runtime_Name(),
		Renegade_Text_Arch_Name(),
		(unsigned)(sizeof(void *) * 8u),
		(unsigned)sizeof(void *),
		(unsigned)sizeof(size_t),
		(unsigned)sizeof(long),
		(unsigned)sizeof(WCHAR),
		(unsigned)sizeof(int),
#if defined(__SIZEOF_WCHAR_T__)
		(int)(__SIZEOF_WCHAR_T__ == 2)
#else
		(int)(sizeof(WCHAR) == 2)
#endif
	);
	Renegade_Agent_Log("T", "renegade_text_log.h", "platform_layout", json);
}

static inline void Renegade_Text_Log_Micro_Chunk_Layout(unsigned mc_header_bytes, unsigned mc_wire_bytes)
{
	static int logged = 0;
	if (logged) {
		return;
	}
	logged = 1;

	char json[96];
	snprintf(
		json,
		sizeof(json),
		"{\"mc_header\":%u,\"mc_wire\":%u}",
		mc_header_bytes,
		mc_wire_bytes);
	Renegade_Agent_Log("T", "chunkio.cpp", "micro_chunk_layout", json);
}

static inline void Renegade_Text_Log_Tdb_String(
	const char *id_desc,
	int chunk_index,
	unsigned byte_len,
	const WCHAR *text)
{
	static int logged = 0;
	if (logged >= 8) {
		return;
	}
	logged++;

	char preview[72];
	Renegade_Text_Wide_Preview(text, preview, (int)sizeof(preview));

	const int len = Renegade_Text_Wide_Len(text);
	char json[320];
	snprintf(
		json,
		sizeof(json),
		"{\"id\":\"%s\",\"chunk\":%d,\"bytes\":%u,\"len\":%d,"
		"\"cp0\":%u,\"cp1\":%u,\"cp2\":%u,\"text\":\"%.60s\"}",
		id_desc != NULL ? id_desc : "",
		chunk_index,
		byte_len,
		len,
		len > 0 ? (unsigned)text[0] : 0u,
		len > 1 ? (unsigned)text[1] : 0u,
		len > 2 ? (unsigned)text[2] : 0u,
		preview);
	Renegade_Agent_Log("T", "translateobj.cpp:Load", "tdb_string", json);
}

static inline void Renegade_Text_Log_Dialog(const char *id_desc, const WCHAR *result)
{
	static int logged = 0;
	if (logged >= 12) {
		return;
	}
	logged++;

	char preview[72];
	Renegade_Text_Wide_Preview(result, preview, (int)sizeof(preview));

	const bool missing =
		(result == NULL) ||
		(result[0] == L'T' && result[1] == L'D' && result[2] == L'B' && result[3] == L'E');

	char json[320];
	snprintf(
		json,
		sizeof(json),
		"{\"id\":\"%s\",\"missing\":%d,\"len\":%d,"
		"\"cp0\":%u,\"cp1\":%u,\"text\":\"%.60s\"}",
		id_desc != NULL ? id_desc : "",
		missing ? 1 : 0,
		Renegade_Text_Wide_Len(result),
		result != NULL && result[0] ? (unsigned)result[0] : 0u,
		result != NULL && result[1] ? (unsigned)result[1] : 0u,
		preview);
	Renegade_Agent_Log("T", "dialogparser.cpp", "dialog_translate", json);
}

static inline void Renegade_Text_Log_Font_Create(
	const char *requested_name,
	const char *resolved_name,
	int point_size,
	int char_height,
	int bitmap_w,
	const char *backend)
{
	static int logged = 0;
	if (logged >= 10) {
		return;
	}
	logged++;

	char req[48];
	char res[48];
	Renegade_Text_Escape_Json(requested_name, req, (int)sizeof(req));
	Renegade_Text_Escape_Json(resolved_name, res, (int)sizeof(res));

	char json[256];
	snprintf(
		json,
		sizeof(json),
		"{\"req\":\"%s\",\"resolved\":\"%s\",\"pt\":%d,\"char_h\":%d,"
		"\"bmp_w\":%d,\"backend\":\"%s\"}",
		req,
		res,
		point_size,
		char_height,
		bitmap_w,
		backend != NULL ? backend : "?");
	Renegade_Agent_Log("T", "render2dsentence.cpp", "font_create", json);
}

static inline void Renegade_Text_Log_Glyph(
	unsigned ch,
	int glyph_w,
	int char_h,
	int max_px,
	int stored_w)
{
	static int logged = 0;
	if (logged >= 20) {
		return;
	}
	logged++;

	char json[160];
	snprintf(
		json,
		sizeof(json),
		"{\"ch\":%u,\"glyph_w\":%d,\"char_h\":%d,\"max_px\":%d,\"stored_w\":%d}",
		ch,
		glyph_w,
		char_h,
		max_px,
		stored_w);
	Renegade_Agent_Log("T", "render2dsentence.cpp", "glyph_store", json);
}

static inline void Renegade_Text_Log_Menu_Entry(
	const WCHAR *title,
	float extent_x,
	float extent_y,
	int char_h)
{
	static int logged = 0;
	if (logged >= 12) {
		return;
	}
	logged++;

	char preview[72];
	Renegade_Text_Wide_Preview(title, preview, (int)sizeof(preview));

	char json[280];
	snprintf(
		json,
		sizeof(json),
		"{\"len\":%d,\"extent_x\":%.1f,\"extent_y\":%.1f,\"char_h\":%d,"
		"\"cp0\":%u,\"cp1\":%u,\"text\":\"%.60s\"}",
		Renegade_Text_Wide_Len(title),
		extent_x,
		extent_y,
		char_h,
		title != NULL && title[0] ? (unsigned)title[0] : 0u,
		title != NULL && title[1] ? (unsigned)title[1] : 0u,
		preview);
	Renegade_Agent_Log("T", "menuentryctrl.cpp", "menu_entry_text", json);
}

static inline void Renegade_Text_Log_Style_Font(
	int font_index,
	const char *name,
	int point_size,
	int char_height)
{
	static int logged = 0;
	if (logged >= 8) {
		return;
	}
	logged++;

	char safe_name[48];
	Renegade_Text_Escape_Json(name, safe_name, (int)sizeof(safe_name));

	char json[192];
	snprintf(
		json,
		sizeof(json),
		"{\"idx\":%d,\"name\":\"%s\",\"pt\":%d,\"char_h\":%d}",
		font_index,
		safe_name,
		point_size,
		char_height);
	Renegade_Agent_Log("T", "stylemgr.cpp", "style_font", json);
}
// #endregion

#else /* !RENEGADE_BOOT_LOG */

#define Renegade_Text_Log_Platform() ((void)0)
#define Renegade_Text_Log_Micro_Chunk_Layout(mc_header_bytes, mc_wire_bytes) ((void)0)
#define Renegade_Text_Log_Tdb_String(id_desc, chunk_index, byte_len, text) ((void)0)
#define Renegade_Text_Log_Dialog(id_desc, result) ((void)0)
#define Renegade_Text_Log_Font_Create(requested_name, resolved_name, point_size, char_height, bitmap_w, backend) \
	((void)0)
#define Renegade_Text_Log_Glyph(ch, glyph_w, char_h, max_px, stored_w) ((void)0)
#define Renegade_Text_Log_Menu_Entry(title, extent_x, extent_y, char_h) ((void)0)
#define Renegade_Text_Log_Style_Font(font_index, name, point_size, char_height) ((void)0)

#endif /* RENEGADE_BOOT_LOG */

#endif /* RENEGADE_TEXT_LOG_H */
