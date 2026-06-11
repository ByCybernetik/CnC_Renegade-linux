/*
** Stubs for symbols defined in renegade.exe but referenced by platform_lib
** when linking scripts.so (scripts.dll is loaded by the game, not standalone).
*/

#include <stdint.h>

extern "C" void Renegade_Stop_Main_Loop(int)
{
}

extern const uint8_t _binary_src_commando_renegade_chat_rsrc_bin_start[] = { 0 };
extern const uint8_t _binary_src_commando_renegade_chat_rsrc_bin_end[] = { 0 };
