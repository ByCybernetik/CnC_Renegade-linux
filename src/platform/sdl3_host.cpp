/*
 * SDL3 + dxvk-native (DXVK_WSI_DRIVER=SDL3): game window and event source.
 */
#include "sdl3_host.h"
#include "pe_resource_loader.h"
#include "linux/winuser.h"
#include "linux/winuser_extra.h"
#include "linux/shellapi.h"

#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif

static SDL_Window *g_window = NULL;
static bool g_quit_posted = false;
static Platform_Window_Message_Handler g_wnd_msg_handler = NULL;
static Platform_Window_Resize_Handler g_resize_handler = NULL;

/* 256-byte keyboard state indexed by Win32 VK (subset used by WWKeyboard). */
static unsigned char g_key_down[256];
static long g_wheel_accum = 0;
static int g_text_input_refs = 0;

static void platform_dispatch_utf8_text(const char *utf8)
{
	const unsigned char *p;
	uint32_t cp;

	if (utf8 == NULL || g_wnd_msg_handler == NULL) {
		return;
	}

	p = (const unsigned char *)utf8;
	while (*p != '\0') {
		cp = 0;
		if ((*p & 0x80u) == 0u) {
			cp = *p++;
		} else if ((*p & 0xE0u) == 0xC0u && p[1] != '\0') {
			cp = ((uint32_t)(p[0] & 0x1Fu) << 6) | (uint32_t)(p[1] & 0x3Fu);
			p += 2;
		} else if ((*p & 0xF0u) == 0xE0u && p[1] != '\0' && p[2] != '\0') {
			cp = ((uint32_t)(p[0] & 0x0Fu) << 12) |
				((uint32_t)(p[1] & 0x3Fu) << 6) |
				(uint32_t)(p[2] & 0x3Fu);
			p += 3;
		} else if ((*p & 0xF8u) == 0xF0u && p[1] != '\0' && p[2] != '\0' && p[3] != '\0') {
			cp = ((uint32_t)(p[0] & 0x07u) << 18) |
				((uint32_t)(p[1] & 0x3Fu) << 12) |
				((uint32_t)(p[2] & 0x3Fu) << 6) |
				(uint32_t)(p[3] & 0x3Fu);
			p += 4;
		} else {
			++p;
			continue;
		}

		if (cp <= 0xFFFFu && g_window != NULL) {
			g_wnd_msg_handler((HWND)g_window, WM_CHAR, (WPARAM)cp, 0);
		}
	}
}

void Platform_Set_System_Cursor_Visible(bool visible)
{
	if (visible) {
		SDL_ShowCursor();
	} else {
		SDL_HideCursor();
	}
}

void Platform_Set_Text_Input_Enabled(bool enabled)
{
	if (g_window == NULL) {
		return;
	}

	if (enabled) {
		if (g_text_input_refs++ == 0) {
			SDL_StartTextInput(g_window);
		}
	} else if (g_text_input_refs > 0) {
		if (--g_text_input_refs == 0) {
			SDL_StopTextInput(g_window);
		}
	}
}

extern HWND MainWindow;
extern HINSTANCE ProgramInstance;
extern bool GameInFocus;

extern "C" void Renegade_Stop_Main_Loop(int exitCode);

void Platform_Init_Early(void)
{
	if (!getenv("DXVK_WSI_DRIVER")) {
		setenv("DXVK_WSI_DRIVER", "SDL3", 0);
	}
	Renegade_Init_Embedded_Resources();
}

int Platform_Init_Video_Audio(void)
{
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		return 0;
	}

	uint32_t window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#if defined(RENEGADE_VULKAN)
	window_flags |= SDL_WINDOW_VULKAN;
#endif
	g_window = SDL_CreateWindow(
		"Renegade",
		800,
		600,
		window_flags);
	if (!g_window) {
		SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
		SDL_Quit();
		return 0;
	}

	SDL_ShowWindow(g_window);
	SDL_RaiseWindow(g_window);
	SDL_HideCursor();
	for (int i = 0; i < 8; ++i) {
		SDL_PumpEvents();
	}

	MainWindow = (HWND)g_window;
	ProgramInstance = (HINSTANCE)1;
	GameInFocus = true;
	return 1;
}

SDL_Window *Platform_Get_SDL_Window(void)
{
	return g_window;
}

HWND Platform_Get_Main_HWnd(void)
{
	return (HWND)g_window;
}

void Platform_Set_Window_Message_Handler(Platform_Window_Message_Handler handler)
{
	g_wnd_msg_handler = handler;
}

void Platform_Set_Async_Key(int vkey, bool down)
{
	if (vkey >= 0 && vkey < 256) {
		g_key_down[vkey] = down ? 1 : 0;
	}
}

bool Platform_Get_Async_Key(int vkey)
{
	if (vkey < 0 || vkey >= 256) {
		return false;
	}
	return g_key_down[vkey] != 0;
}

void Platform_Accumulate_Mouse_Wheel(float delta_y)
{
	g_wheel_accum += (long)(delta_y * 120.0f);
}

long Platform_Consume_Mouse_Wheel(void)
{
	const long wheel = g_wheel_accum;
	g_wheel_accum = 0;
	return wheel;
}

static int sdl_scancode_to_vk(SDL_Scancode sc)
{
	/* SDL3 keyboard scancode → Win32 virtual key (US layout subset). */
	switch (sc) {
	case SDL_SCANCODE_ESCAPE: return VK_ESCAPE;
	case SDL_SCANCODE_RETURN: return VK_RETURN;
	case SDL_SCANCODE_SPACE: return VK_SPACE;
	case SDL_SCANCODE_LSHIFT: return 0xA0;
	case SDL_SCANCODE_RSHIFT: return 0xA1;
	case SDL_SCANCODE_LCTRL: return 0xA2;
	case SDL_SCANCODE_RCTRL: return 0xA3;
	case SDL_SCANCODE_LALT: return 0xA4;
	case SDL_SCANCODE_RALT: return 0xA5;
	case SDL_SCANCODE_CAPSLOCK: return VK_CAPITAL;
	case SDL_SCANCODE_TAB: return VK_TAB;
	case SDL_SCANCODE_BACKSPACE: return VK_BACK;
	case SDL_SCANCODE_DELETE: return VK_DELETE;
	case SDL_SCANCODE_HOME: return VK_HOME;
	case SDL_SCANCODE_END: return VK_END;
	case SDL_SCANCODE_PAGEUP: return VK_PRIOR;
	case SDL_SCANCODE_PAGEDOWN: return VK_NEXT;
	case SDL_SCANCODE_UP: return VK_UP;
	case SDL_SCANCODE_DOWN: return VK_DOWN;
	case SDL_SCANCODE_LEFT: return VK_LEFT;
	case SDL_SCANCODE_RIGHT: return VK_RIGHT;
	case SDL_SCANCODE_INSERT: return VK_INSERT;
	case SDL_SCANCODE_KP_ENTER: return VK_RETURN;
	case SDL_SCANCODE_MINUS: return 0xBD;
	case SDL_SCANCODE_EQUALS: return 0xBB;
	case SDL_SCANCODE_LEFTBRACKET: return 0xDB;
	case SDL_SCANCODE_RIGHTBRACKET: return 0xDD;
	case SDL_SCANCODE_BACKSLASH: return 0xDC;
	case SDL_SCANCODE_SEMICOLON: return 0xBA;
	case SDL_SCANCODE_APOSTROPHE: return 0xDE;
	case SDL_SCANCODE_GRAVE: return 0xC0;
	case SDL_SCANCODE_COMMA: return 0xBC;
	case SDL_SCANCODE_PERIOD: return 0xBE;
	case SDL_SCANCODE_SLASH: return 0xBF;
	default:
		if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) {
			return 'A' + (sc - SDL_SCANCODE_A);
		}
		if (sc >= SDL_SCANCODE_0 && sc <= SDL_SCANCODE_9) {
			return '0' + (sc - SDL_SCANCODE_0);
		}
		if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F12) {
			return 0x70 + (sc - SDL_SCANCODE_F1);
		}
		return 0;
	}
}

extern void (*Win32_Key_Notify_Callback_Ptr)(unsigned int message, unsigned int wParam, long lParam);

static void Platform_Handle_Quit_Request(void)
{
	if (g_quit_posted) {
		return;
	}
	g_quit_posted = true;
	Renegade_Stop_Main_Loop(EXIT_SUCCESS);
}

BOOL Platform_Show_Window(HWND hwnd, int cmdShow)
{
	SDL_Window *window = (SDL_Window *)hwnd;
	if (window == NULL) {
		window = g_window;
	}
	if (window == NULL) {
		return FALSE;
	}

	switch (cmdShow) {
	case SW_HIDE:
	case SW_MINIMIZE:
		SDL_HideWindow(window);
		SDL_MinimizeWindow(window);
		break;
	default:
		SDL_ShowWindow(window);
		SDL_RaiseWindow(window);
		break;
	}

	for (int i = 0; i < 4; ++i) {
		SDL_PumpEvents();
	}
	return TRUE;
}

void Platform_Set_Window_Resize_Handler(Platform_Window_Resize_Handler handler)
{
	g_resize_handler = handler;
}

void Platform_Pump_Events(void)
{
	SDL_Event ev;
	while (SDL_PollEvent(&ev)) {
		switch (ev.type) {
		case SDL_EVENT_QUIT:
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			Platform_Handle_Quit_Request();
			break;

		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			if (g_resize_handler != NULL &&
				ev.window.windowID == SDL_GetWindowID(g_window)) {
				g_resize_handler((int)ev.window.data1, (int)ev.window.data2);
			}
			break;

		case SDL_EVENT_WINDOW_FOCUS_GAINED:
			GameInFocus = true;
			SDL_HideCursor();
			if (g_text_input_refs > 0) {
				SDL_StartTextInput(g_window);
			}
			break;
		case SDL_EVENT_WINDOW_FOCUS_LOST:
			GameInFocus = false;
			SDL_ShowCursor();
			break;

		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP: {
			const int vk = sdl_scancode_to_vk(ev.key.scancode);
			if (vk) {
				const bool down = (ev.type == SDL_EVENT_KEY_DOWN);
				Platform_Set_Async_Key(vk, down);
				if (Win32_Key_Notify_Callback_Ptr) {
					const UINT wm = down ? WM_KEYDOWN : WM_KEYUP;
					const LPARAM lp = (ev.key.repeat ? (LPARAM)KF_REPEAT : 0);
					Win32_Key_Notify_Callback_Ptr(wm, (WPARAM)vk, lp);
				}
				if (g_wnd_msg_handler != NULL) {
					const UINT wm = down ? WM_KEYDOWN : WM_KEYUP;
					const LPARAM lp = (ev.key.repeat ? (LPARAM)KF_REPEAT : 0);
					g_wnd_msg_handler(MainWindow, wm, (WPARAM)vk, lp);
				}
			}
			break;
		}

		case SDL_EVENT_TEXT_INPUT:
			platform_dispatch_utf8_text(ev.text.text);
			break;

		case SDL_EVENT_MOUSE_WHEEL:
			Platform_Accumulate_Mouse_Wheel(ev.wheel.y);
			break;

		default:
			break;
		}
	}
}

void Platform_Pre_Shutdown(void)
{
	GameInFocus = false;
	if (g_window) {
		SDL_HideWindow(g_window);
	}
	for (int i = 0; i < 8; ++i) {
		SDL_PumpEvents();
	}
}

void Platform_Shutdown(void)
{
	MainWindow = NULL;
	if (g_window) {
		SDL_DestroyWindow(g_window);
		g_window = NULL;
	}
	for (int i = 0; i < 4; ++i) {
		SDL_PumpEvents();
	}
	/* SDL_Quit can deadlock with dxvk-native after D3D8 teardown. */
}
