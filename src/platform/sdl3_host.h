/*
 * SDL3 host: window, event pump, dxvk-native WSI bootstrap.
 */
#ifndef RENEGADE_SDL3_HOST_H
#define RENEGADE_SDL3_HOST_H

#include <windows.h>
#include "linux/win32_minimal.h"

struct SDL_Window;

#ifdef __cplusplus
extern "C" {
#endif

void Platform_Init_Early(void);
int Platform_Init_Video_Audio(void);
SDL_Window *Platform_Get_SDL_Window(void);
HWND Platform_Get_Main_HWnd(void);
void Platform_Pump_Events(void);
BOOL Platform_Show_Window(HWND hwnd, int cmdShow);
void Platform_Pre_Shutdown(void);
void Platform_Shutdown(void);

void Platform_Set_Async_Key(int vkey, bool down);
bool Platform_Get_Async_Key(int vkey);

void Platform_Accumulate_Mouse_Wheel(float delta_y);
long Platform_Consume_Mouse_Wheel(void);
void Platform_Set_Text_Input_Enabled(bool enabled);
void Platform_Set_System_Cursor_Visible(bool visible);

#ifdef __cplusplus
typedef long (*Platform_Window_Message_Handler)(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
void Platform_Set_Window_Message_Handler(Platform_Window_Message_Handler handler);

typedef void (*Platform_Window_Resize_Handler)(int width, int height);
void Platform_Set_Window_Resize_Handler(Platform_Window_Resize_Handler handler);
#endif

#ifdef __cplusplus
}
#endif

#endif
