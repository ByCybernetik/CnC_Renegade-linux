/*
 * Linux program entry: SDL3 host + WinMain.
 */
#include "linux/win32_minimal.h"
#include "sdl3_host.h"
#include <stdlib.h>
#include <cstring>
#include <unistd.h>

extern int PASCAL WinMain(HINSTANCE, HINSTANCE, LPSTR, int);

int main(int argc, char **argv)
{
	(void)argc;
	Platform_Init_Early();

	char cmdline[4096];
	cmdline[0] = '\0';
	for (int i = 1; i < argc; ++i) {
		if (i > 1) {
			strcat(cmdline, " ");
		}
		strcat(cmdline, argv[i]);
	}

	if (!Platform_Init_Video_Audio()) {
		return 1;
	}

	const int exit_code = WinMain((HINSTANCE)1, NULL, cmdline, 1);
	Platform_Shutdown();
	/* dxvk-native worker threads may not exit cleanly; avoid hanging in static dtors. */
	_exit(exit_code);
}
