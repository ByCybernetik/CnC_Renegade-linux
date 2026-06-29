/*
 * Stubs for standalone WWAudioClass test player.
 * Provides minimal implementations of symbols required by wwaudio
 * that are normally supplied by ww3d2, commando, or Win32.
 */

#include "win32_minimal.h"

/* ---- Win32 globals expected by wwlib/ww3d2 ---- */
HWND MainWindow = NULL;
HINSTANCE ProgramInstance = NULL;
bool GameInFocus = false;

/* ---- Renegade main loop stop (unused) ---- */
extern "C" void Renegade_Stop_Main_Loop(int) {}

/* ---- Binary resource stubs (linked by commando) ---- */
extern const unsigned char _binary_src_commando_renegade_chat_rsrc_bin_start[] = {0};
extern const unsigned char _binary_src_commando_renegade_chat_rsrc_bin_end[] = {0};

/* ---- WW3D statics (needed by audiblesound.cpp via ww3d.h Get_Frame_Time) ---- */
#include "ww3d.h"
unsigned int WW3D::SyncTime = 0;
unsigned int WW3D::PreviousSyncTime = 0;
int WW3D::FrameCount = 0;
