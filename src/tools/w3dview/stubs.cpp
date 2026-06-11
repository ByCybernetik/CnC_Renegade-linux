#include "win32_minimal.h"

extern "C" void Renegade_Stop_Main_Loop(int)
{
}

extern const unsigned char _binary_src_commando_renegade_chat_rsrc_bin_start[] = {0};
extern const unsigned char _binary_src_commando_renegade_chat_rsrc_bin_end[] = {0};

HWND MainWindow = NULL;
HINSTANCE ProgramInstance = NULL;
bool GameInFocus = false;
