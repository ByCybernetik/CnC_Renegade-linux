#include <windows.h>
#include "renegade_win32_shim.h"
#include "verchk.h"
#include <string.h>

bool GetVersionInfo(char *filename, VS_FIXEDFILEINFO *fileInfo)
{
	if (fileInfo) {
		memset(fileInfo, 0, sizeof(*fileInfo));
	}
	(void)filename;
	return false;
}

bool GetFileCreationTime(char *filename, FILETIME *createTime)
{
	if (createTime) {
		createTime->dwLowDateTime = 0;
		createTime->dwHighDateTime = 0;
	}
	(void)filename;
	return false;
}

int Compare_EXE_Version(int, const char *)
{
	return 0;
}
