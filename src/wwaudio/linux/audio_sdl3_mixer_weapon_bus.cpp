/*
** Weapon bus: 3D combat SFX + 2D gunshots/reload (non-menu voices).
*/

#include "audio_sdl3_mixer_weapon_bus.h"
#include "sdl3_mix_export.h"

#include <SDL3/SDL.h>
#include <string.h>

struct Sdl3MixerWeaponBus
{
	int output_rate;
	int channels;
	bool ready;
};

static Sdl3MixerWeaponBus g_weapon_bus;

extern "C" {

void sdl3_mixer_weapon_bus_init(int output_rate, int channels)
{
	sdl3_mixer_weapon_bus_shutdown();

	g_weapon_bus.output_rate = (output_rate > 0) ? output_rate : 48000;
	g_weapon_bus.channels = (channels > 0) ? channels : 2;
	g_weapon_bus.ready = true;
}

void sdl3_mixer_weapon_bus_shutdown(void)
{
	memset(&g_weapon_bus, 0, sizeof(g_weapon_bus));
}

bool sdl3_mixer_weapon_bus_ready(void)
{
	return g_weapon_bus.ready;
}

void sdl3_mixer_weapon_bus_render(float *mix_l, float *mix_r, int frames, int output_rate)
{
	if (!g_weapon_bus.ready) {
		return;
	}
	sdl3_mix_weapon_voices(mix_l, mix_r, frames, output_rate);
}

} /* extern "C" */
