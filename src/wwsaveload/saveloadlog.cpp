#include "saveloadlog.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#ifdef SAVELOAD_DEBUG

FILE *SaveLoadLog::file_ = NULL;
int SaveLoadLog::indent_level_ = 0;
bool SaveLoadLog::opened_ = false;

void SaveLoadLog::Open()
{
	if (opened_) return;
	opened_ = true;
	file_ = fopen("saveload_debug.txt", "w");
	if (file_) {
		time_t t = time(NULL);
		struct tm *tm = localtime(&t);
		fprintf(file_, "=== Save/Load Debug Log === %04d-%02d-%02d %02d:%02d:%02d\n",
			tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec);
		fflush(file_);
	}
}

void SaveLoadLog::Close()
{
	if (file_) {
		fprintf(file_, "=== End of log ===\n");
		fclose(file_);
		file_ = NULL;
	}
	opened_ = false;
	indent_level_ = 0;
}

void SaveLoadLog::Log(const char *format, ...)
{
	if (!file_) return;
	for (int i = 0; i < indent_level_; i++) {
		fputc(' ', file_);
	}
	va_list args;
	va_start(args, format);
	vfprintf(file_, format, args);
	va_end(args);
	fputc('\n', file_);
	fflush(file_);
}

void SaveLoadLog::Log_Hex(const char *label, const void *data, uint32 size)
{
	if (!file_) return;
	for (int i = 0; i < indent_level_; i++) fputc(' ', file_);
	fprintf(file_, "%s: ", label);
	const unsigned char *bytes = (const unsigned char *)data;
	for (uint32 i = 0; i < size; i++) {
		fprintf(file_, "%02x ", bytes[i]);
	}
	fputc('\n', file_);
}

void SaveLoadLog::Indent()
{
	indent_level_ += 2;
}

void SaveLoadLog::Unindent()
{
	if (indent_level_ >= 2) indent_level_ -= 2;
}

void SaveLoadLog::Flush()
{
	if (file_) fflush(file_);
}

#endif
