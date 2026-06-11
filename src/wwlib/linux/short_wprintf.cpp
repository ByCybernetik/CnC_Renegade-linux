/*
 * Wide printf for Linux builds with -fshort-wchar (16-bit WCHAR).
 * glibc vswprintf writes 32-bit wchar_t and overflows WCHAR buffers.
 */

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include "ww_wcstring.h"

namespace {

static bool ptr_ok(const void *p)
{
	return (p != NULL) && ((uintptr_t)p > 1u);
}

static void wput_char(WCHAR *buf, size_t count, size_t *out, WCHAR ch)
{
	if (*out + 1 < count) {
		buf[(*out)++] = ch;
	}
}

static void wput_pad(WCHAR *buf, size_t count, size_t *out, WCHAR pad, int n)
{
	for (int i = 0; i < n; ++i) {
		wput_char(buf, count, out, pad);
	}
}

static void wput_wstr(WCHAR *buf, size_t count, size_t *out, const WCHAR *s)
{
	if (!ptr_ok(s)) {
		s = L"";
	}
	for (size_t i = 0; s[i] != 0; ++i) {
		wput_char(buf, count, out, s[i]);
	}
}

static void wput_astr(WCHAR *buf, size_t count, size_t *out, const char *s)
{
	if (!ptr_ok(s)) {
		s = "";
	}
	for (size_t i = 0; s[i] != '\0'; ++i) {
		wput_char(buf, count, out, (WCHAR)(unsigned char)s[i]);
	}
}

static void wput_wstr_padded(
	WCHAR *buf,
	size_t count,
	size_t *out,
	const WCHAR *s,
	int width,
	bool left,
	WCHAR pad)
{
	if (!ptr_ok(s)) {
		s = L"";
	}
	const size_t slen = WW_WCSTRLEN(s);
	const int pad_count = (width > (int)slen) ? (width - (int)slen) : 0;

	if (!left) {
		wput_pad(buf, count, out, pad, pad_count);
	}
	wput_wstr(buf, count, out, s);
	if (left) {
		wput_pad(buf, count, out, pad, pad_count);
	}
}

static void wput_astr_padded(
	WCHAR *buf,
	size_t count,
	size_t *out,
	const char *s,
	int width,
	bool left,
	WCHAR pad)
{
	if (!ptr_ok(s)) {
		s = "";
	}
	size_t slen = strlen(s);
	const int pad_count = (width > (int)slen) ? (width - (int)slen) : 0;

	if (!left) {
		wput_pad(buf, count, out, pad, pad_count);
	}
	wput_astr(buf, count, out, s);
	if (left) {
		wput_pad(buf, count, out, pad, pad_count);
	}
}

static void append_ascii_fmt_char(char *dst, size_t dst_size, size_t *len, char ch)
{
	if (*len + 1 < dst_size) {
		dst[(*len)++] = ch;
		dst[*len] = '\0';
	}
}

static void append_ascii_fmt_str(char *dst, size_t dst_size, size_t *len, const char *s)
{
	while (*s != '\0' && *len + 1 < dst_size) {
		dst[(*len)++] = *s++;
	}
	dst[*len] = '\0';
}

static void append_ascii_fmt_int(char *dst, size_t dst_size, size_t *len, int value)
{
	char tmp[16];
	snprintf(tmp, sizeof(tmp), "%d", value);
	append_ascii_fmt_str(dst, dst_size, len, tmp);
}

} // namespace

extern "C" int WW_VSNWPRINTF(WCHAR *buffer, size_t count, const WCHAR *format, va_list args)
{
	if (buffer == NULL || count == 0 || !ptr_ok(format)) {
		return -1;
	}

	size_t out = 0;
	va_list ap;
	va_copy(ap, args);

	for (size_t fi = 0; format[fi] != 0; ++fi) {
		const WCHAR ch = format[fi];
		if (ch != L'%') {
			wput_char(buffer, count, &out, ch);
			continue;
		}

		++fi;
		if (format[fi] == 0) {
			break;
		}
		if (format[fi] == L'%') {
			wput_char(buffer, count, &out, L'%');
			continue;
		}

		bool left = false;
		WCHAR pad = L' ';
		if (format[fi] == L'-') {
			left = true;
			++fi;
		}
		if (format[fi] == L'0') {
			pad = L'0';
			++fi;
		}

		int width = 0;
		while (format[fi] >= L'0' && format[fi] <= L'9') {
			width = width * 10 + (format[fi] - L'0');
			++fi;
		}

		int prec = -1;
		if (format[fi] == L'.') {
			++fi;
			prec = 0;
			while (format[fi] >= L'0' && format[fi] <= L'9') {
				prec = prec * 10 + (format[fi] - L'0');
				++fi;
			}
		}

		char length_mod = 0;
		if (format[fi] == L'l' || format[fi] == L'h') {
			length_mod = (char)format[fi];
			++fi;
		}

		const WCHAR spec = format[fi];
		char ansi_fmt[64];
		size_t af_len = 0;
		ansi_fmt[0] = '%';
		ansi_fmt[1] = '\0';
		af_len = 1;

		if (left) {
			append_ascii_fmt_char(ansi_fmt, sizeof(ansi_fmt), &af_len, '-');
		}
		if (pad == L'0' && !left) {
			append_ascii_fmt_char(ansi_fmt, sizeof(ansi_fmt), &af_len, '0');
		}
		if (width > 0) {
			append_ascii_fmt_int(ansi_fmt, sizeof(ansi_fmt), &af_len, width);
		}
		if (prec >= 0) {
			append_ascii_fmt_char(ansi_fmt, sizeof(ansi_fmt), &af_len, '.');
			append_ascii_fmt_int(ansi_fmt, sizeof(ansi_fmt), &af_len, prec);
		}
		if (length_mod != 0) {
			append_ascii_fmt_char(ansi_fmt, sizeof(ansi_fmt), &af_len, length_mod);
		}

		switch (spec) {
		case L'd':
		case L'i': {
			append_ascii_fmt_char(ansi_fmt, sizeof(ansi_fmt), &af_len, 'd');
			char tmp[128];
			if (length_mod == 'l') {
				const long value = va_arg(ap, long);
				snprintf(tmp, sizeof(tmp), ansi_fmt, value);
			} else {
				const int value = va_arg(ap, int);
				snprintf(tmp, sizeof(tmp), ansi_fmt, value);
			}
			wput_astr(buffer, count, &out, tmp);
			break;
		}

		case L'u': {
			append_ascii_fmt_char(ansi_fmt, sizeof(ansi_fmt), &af_len, 'u');
			if (length_mod == 'l') {
				const unsigned long value = va_arg(ap, unsigned long);
				char tmp[128];
				snprintf(tmp, sizeof(tmp), ansi_fmt, value);
				wput_astr(buffer, count, &out, tmp);
			} else {
				const unsigned int value = va_arg(ap, unsigned int);
				char tmp[128];
				snprintf(tmp, sizeof(tmp), ansi_fmt, value);
				wput_astr(buffer, count, &out, tmp);
			}
			break;
		}

		case L'f':
		case L'g': {
			append_ascii_fmt_char(ansi_fmt, sizeof(ansi_fmt), &af_len, (char)spec);
			const double value = va_arg(ap, double);
			char tmp[128];
			snprintf(tmp, sizeof(tmp), ansi_fmt, value);
			wput_astr(buffer, count, &out, tmp);
			break;
		}

		case L'c': {
			const WCHAR value = (WCHAR)va_arg(ap, int);
			wput_wstr_padded(buffer, count, &out, &value, width, left, pad);
			break;
		}

		case L's': {
			const WCHAR *value = va_arg(ap, const WCHAR *);
			wput_wstr_padded(buffer, count, &out, value, width, left, pad);
			break;
		}

		case L'S': {
			const char *value = va_arg(ap, const char *);
			wput_astr_padded(buffer, count, &out, value, width, left, pad);
			break;
		}

		default:
			wput_char(buffer, count, &out, L'%');
			if (left) {
				wput_char(buffer, count, &out, L'-');
			}
			if (pad == L'0') {
				wput_char(buffer, count, &out, L'0');
			}
			wput_char(buffer, count, &out, spec);
			break;
		}
	}

	va_end(ap);

	if (out < count) {
		buffer[out] = 0;
	} else if (count > 0) {
		buffer[count - 1] = 0;
	}

	return (int)out;
}
