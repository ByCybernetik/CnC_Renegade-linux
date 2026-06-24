#ifndef SAVELOADLOG_H
#define SAVELOADLOG_H

#include "always.h"
#include "bittype.h"

#ifdef SAVELOAD_DEBUG

class SaveLoadLog {
public:
	static void Open();
	static void Close();
	static void Log(const char *format, ...);
	static void Log_Hex(const char *label, const void *data, uint32 size);
	static void Indent();
	static void Unindent();
	static void Flush();

private:
	static FILE *file_;
	static int indent_level_;
	static bool opened_;
};

#define SAVELOAD_LOG(fmt, ...)       do { SaveLoadLog::Log(fmt, ##__VA_ARGS__); } while(0)
#define SAVELOAD_OPEN()              do { SaveLoadLog::Open(); } while(0)
#define SAVELOAD_CLOSE()             do { SaveLoadLog::Close(); } while(0)
#define SAVELOAD_INDENT()            do { SaveLoadLog::Indent(); } while(0)
#define SAVELOAD_UNINDENT()          do { SaveLoadLog::Unindent(); } while(0)
#define SAVELOAD_FLUSH()             do { SaveLoadLog::Flush(); } while(0)

#else

#define SAVELOAD_LOG(fmt, ...)
#define SAVELOAD_OPEN()
#define SAVELOAD_CLOSE()
#define SAVELOAD_INDENT()
#define SAVELOAD_UNINDENT()
#define SAVELOAD_FLUSH()

#endif

#endif
