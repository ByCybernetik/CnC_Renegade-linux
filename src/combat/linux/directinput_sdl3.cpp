/*
 * DirectInput → SDL3 keyboard/mouse/gamepad (Linux port).
 */
#include "directinput.h"
#include "timemgr.h"
#include "msgloop.h"
#include "../../platform/sdl3_host.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#define BUTTON_DOUBLE_THRESHHOLD 0.25f
#define BUTTON_BIT_DOUBLE 0x0008

char DirectInput::DIKeyboardButtons[DirectInput::NUM_KEYBOARD_BUTTONS];
char DirectInput::DIMouseButtons[DirectInput::NUM_MOUSE_BUTTONS];
long DirectInput::DIMouseAxis[DirectInput::NUM_MOUSE_AXIS];
char DirectInput::DIJoystickButtons[DirectInput::NUM_MOUSE_BUTTONS];
float DirectInput::ButtonLastHitTime[DirectInput::NUM_KEYBOARD_BUTTONS];
Vector3 DirectInput::CursorPos(0, 0, 0);
bool DirectInput::EatMouseHeld = false;
void *DirectInput::DirectInputLibrary = NULL;
int DirectInput::LastKeyPressed = 0;
bool DirectInput::Captured = false;

static bool g_prev_key[DirectInput::NUM_KEYBOARD_BUTTONS];
static bool g_menu_mouse_mode = false;

static void sync_mouse_capture_mode(bool captured)
{
	SDL_Window *window = Platform_Get_SDL_Window();
	if (window == NULL) {
		return;
	}

	Platform_Set_System_Cursor_Visible(false);

	if (!captured || g_menu_mouse_mode) {
		SDL_SetWindowRelativeMouseMode(window, false);
		return;
	}

	SDL_SetWindowRelativeMouseMode(window, true);
	float discard_x = 0.0f;
	float discard_y = 0.0f;
	SDL_GetRelativeMouseState(&discard_x, &discard_y);
}

void DirectInput::Set_Menu_Mouse_Mode(bool menu_mode)
{
	g_menu_mouse_mode = menu_mode;
	sync_mouse_capture_mode(Captured);
}

/*
 * Map Win32 virtual key → DirectInput scan code (DIK_*).
 * Input mappings in *.cfg use DIK indices, not VK (see DEFAULT_INPUT / input01.cfg).
 */
static int vk_to_dik(int vk)
{
	/* VK_F1..VK_F12 (0x70..0x7B) overlap lowercase ASCII ('p'..'z'). */
	if (vk >= 0x70 && vk <= 0x7B) {
		return DIK_F1 + (vk - 0x70);
	}

	if (vk >= 'a' && vk <= 'z') {
		vk = vk - 'a' + 'A';
	}

	if (vk >= 'A' && vk <= 'Z') {
		switch (vk) {
		case 'A': return DIK_A;
		case 'B': return DIK_B;
		case 'C': return DIK_C;
		case 'D': return DIK_D;
		case 'E': return DIK_E;
		case 'F': return DIK_F;
		case 'G': return DIK_G;
		case 'H': return DIK_H;
		case 'I': return DIK_I;
		case 'J': return DIK_J;
		case 'K': return DIK_K;
		case 'L': return DIK_L;
		case 'M': return DIK_M;
		case 'N': return DIK_N;
		case 'O': return DIK_O;
		case 'P': return DIK_P;
		case 'Q': return DIK_Q;
		case 'R': return DIK_R;
		case 'S': return DIK_S;
		case 'T': return DIK_T;
		case 'U': return DIK_U;
		case 'V': return DIK_V;
		case 'W': return DIK_W;
		case 'X': return DIK_X;
		case 'Y': return DIK_Y;
		case 'Z': return DIK_Z;
		default: break;
		}
	}

	if (vk >= '1' && vk <= '9') {
		return DIK_1 + (vk - '1');
	}
	if (vk == '0') {
		return DIK_0;
	}

	switch (vk) {
	case 0x1B: return DIK_ESCAPE;
	case 0x0D: return DIK_RETURN;
	case 0x20: return DIK_SPACE;
	case 0x09: return DIK_TAB;
	case 0x08: return DIK_BACK;
	case 0xBD: return DIK_MINUS;
	case 0xBB: return DIK_EQUALS;
	case 0xDB: return DIK_LBRACKET;
	case 0xDD: return DIK_RBRACKET;
	case 0xDC: return DIK_BACKSLASH;
	case 0xBA: return DIK_SEMICOLON;
	case 0xDE: return DIK_APOSTROPHE;
	case 0xC0: return DIK_GRAVE;
	case 0xBC: return DIK_COMMA;
	case 0xBE: return DIK_PERIOD;
	case 0xBF: return DIK_SLASH;
	case 0x14: return DIK_CAPITAL;
	case 0x2D: return DIK_INSERT;
	case 0x2E: return DIK_DELETE;
	case 0x24: return DIK_HOME;
	case 0x23: return DIK_END;
	case 0x21: return DIK_PRIOR;
	case 0x22: return DIK_NEXT;
	case 0x26: return DIK_UP;
	case 0x28: return DIK_DOWN;
	case 0x25: return DIK_LEFT;
	case 0x27: return DIK_RIGHT;
	case 0x90: return DIK_NUMLOCK;
	case 0x91: return DIK_SCROLL;
	case 0x6F: return DIK_DIVIDE;
	case 0x6A: return DIK_MULTIPLY;
	case 0x6D: return DIK_SUBTRACT;
	case 0x6B: return DIK_ADD;
	case 0x6E: return DIK_DECIMAL;
	case 0xA0:
	case 0x10: return DIK_LSHIFT;
	case 0xA1: return DIK_RSHIFT;
	case 0xA2:
	case 0x11: return DIK_LCONTROL;
	case 0xA3: return DIK_RCONTROL;
	case 0xA4:
	case 0x12: return DIK_LMENU;
	case 0xA5: return DIK_RMENU;
	default:
		break;
	}

	/* Numpad digits (VK_NUMPAD0 = 0x60). */
	if (vk >= 0x60 && vk <= 0x69) {
		return DIK_NUMPAD0 + (vk - 0x60);
	}

	return -1;
}

void DirectInput::Init(void)
{
	memset(DIKeyboardButtons, 0, sizeof(DIKeyboardButtons));
	memset(DIMouseButtons, 0, sizeof(DIMouseButtons));
	memset(DIMouseAxis, 0, sizeof(DIMouseAxis));
	memset(DIJoystickButtons, 0, sizeof(DIJoystickButtons));
	memset(ButtonLastHitTime, 0, sizeof(ButtonLastHitTime));
	memset(g_prev_key, 0, sizeof(g_prev_key));
	Captured = false;
	Acquire();
}

void DirectInput::Shutdown(void) {}

void DirectInput::Flush(void)
{
	memset(DIKeyboardButtons, 0, sizeof(DIKeyboardButtons));
	memset(DIMouseButtons, 0, sizeof(DIMouseButtons));
}

void DirectInput::Notify_Win32_Key(UINT message, WPARAM wParam, LPARAM)
{
	const int dik = vk_to_dik((int)wParam);
	if (dik < 0) {
		return;
	}

	const bool down = (message == WM_KEYDOWN || message == WM_SYSKEYDOWN);
	if (down) {
		LastKeyPressed = dik;
	}

	// Edge detection (DI_BUTTON_HIT) is handled in ReadKeyboard() using
	// g_prev_key from the previous frame. Updating g_prev_key here breaks
	// one-shot actions such as F1 help and F8 console.
}

void DirectInput::Acquire(void)
{
	if (Captured) {
		return;
	}

	Flush();

	Platform_Set_System_Cursor_Visible(false);
	Captured = true;
	sync_mouse_capture_mode(Captured);
}

void DirectInput::Unacquire(void)
{
	if (!Captured) {
		return;
	}

	SDL_Window *window = Platform_Get_SDL_Window();
	if (window != NULL) {
		SDL_SetWindowRelativeMouseMode(window, false);
	}

	Captured = false;
}

void DirectInput::ReadKeyboard(void)
{
	bool dik_down[DirectInput::NUM_KEYBOARD_BUTTONS];

	memset(dik_down, 0, sizeof(dik_down));
	for (int vk = 0; vk < DirectInput::NUM_KEYBOARD_BUTTONS; ++vk) {
		const int dik = vk_to_dik(vk);
		if (dik < 0) {
			continue;
		}
		if (Platform_Get_Async_Key(vk) || Platform_Consume_Key_Hit(vk)) {
			dik_down[dik] = true;
		}
	}

	for (int dik = 0; dik < DirectInput::NUM_KEYBOARD_BUTTONS; ++dik) {
		const bool down = dik_down[dik];
		const bool was = g_prev_key[dik];

		if (down && !was) {
			DIKeyboardButtons[dik] = DI_BUTTON_HIT | DI_BUTTON_HELD;
			LastKeyPressed = dik;
		} else if (down) {
			DIKeyboardButtons[dik] = DI_BUTTON_HELD;
		} else if (was) {
			DIKeyboardButtons[dik] = DI_BUTTON_RELEASED;
		} else {
			DIKeyboardButtons[dik] = 0;
		}

		g_prev_key[dik] = down;
	}
}

void DirectInput::ReadMouse(void)
{
	for (int i = 0; i < NUM_MOUSE_AXIS; ++i) {
		DIMouseAxis[i] = 0;
	}

	float rel_x = 0.0f;
	float rel_y = 0.0f;
	SDL_GetRelativeMouseState(&rel_x, &rel_y);
	DIMouseAxis[MOUSE_X_AXIS] = (long)rel_x;
	DIMouseAxis[MOUSE_Y_AXIS] = (long)rel_y;
	DIMouseAxis[MOUSE_Z_AXIS] = Platform_Consume_Mouse_Wheel();

	float x = 0.0f;
	float y = 0.0f;
	const Uint32 buttons = SDL_GetMouseState(&x, &y);

	CursorPos.X = x;
	CursorPos.Y = y;
	CursorPos.X += DIMouseAxis[MOUSE_X_AXIS] * 2.0f;
	CursorPos.Y += DIMouseAxis[MOUSE_Y_AXIS] * 2.0f;

	const bool left = (buttons & SDL_BUTTON_LMASK) != 0;
	const bool right = (buttons & SDL_BUTTON_RMASK) != 0;
	const bool mid = (buttons & SDL_BUTTON_MMASK) != 0;

	auto set_btn = [](char &state, bool held, bool &prev) {
		if (held && !prev) {
			state = DI_BUTTON_HIT | DI_BUTTON_HELD;
		} else if (held) {
			state = DI_BUTTON_HELD;
		} else if (prev) {
			state = DI_BUTTON_RELEASED;
		} else {
			state = 0;
		}
		prev = held;
	};

	static bool pl = false, pr = false, pm = false;
	set_btn(DIMouseButtons[0], left, pl);
	set_btn(DIMouseButtons[1], mid, pm);
	set_btn(DIMouseButtons[2], right, pr);

	if (EatMouseHeld) {
		DIMouseButtons[0] &= ~DI_BUTTON_HELD;
		DIMouseButtons[0] &= ~DI_BUTTON_HIT;
		DIMouseButtons[0] |= DI_BUTTON_RELEASED;
		// Match Win32 DirectInput: stop eating once the menu click is released.
		if (!left) {
			EatMouseHeld = false;
			DIMouseButtons[0] = 0;
			pl = false;
		}
	}
}

void DirectInput::ReadJoystick(void)
{
	/* Gamepad mapping: phase 2 (SDL_GAMEPAD). */
}

void DirectInput::Read(void)
{
	ReadKeyboard();
	if (Captured) {
		ReadMouse();
		ReadJoystick();
		Update_Double_Clicks();
	}
}

void DirectInput::Eat_Mouse_Held_States(void)
{
	if ((DIMouseButtons[0] & DI_BUTTON_HELD) || (DIMouseButtons[0] & DI_BUTTON_HIT)) {
		EatMouseHeld = true;
	}
}

long DirectInput::Get_Joystick_Axis_State(JoystickAxis axis)
{
	(void)axis;
	return 0;
}

void DirectInput::Update_Double_Clicks(void)
{
	float time_delta = TimeManager::Get_Frame_Real_Seconds();
	for (int index = 0; index < DirectInput::NUM_KEYBOARD_BUTTONS; index++) {
		ButtonLastHitTime[index] += time_delta;
		if (DIKeyboardButtons[index] & DI_BUTTON_HIT) {
			if (ButtonLastHitTime[index] <= BUTTON_DOUBLE_THRESHHOLD) {
				DIKeyboardButtons[index] |= BUTTON_BIT_DOUBLE;
			}
			ButtonLastHitTime[index] = 0;
		}
	}
}
