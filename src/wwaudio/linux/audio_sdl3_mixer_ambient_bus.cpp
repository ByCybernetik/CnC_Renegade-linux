/*
** Ambient bus: weather, level beds, static environmental loops.
*/

#include "audio_sdl3_mixer_ambient_bus.h"
#include "sdl3_mix_export.h"

#include <SDL3/SDL.h>
#include <string.h>

struct Sdl3MixerAmbientBus
{
	int output_rate;
	int channels;
	bool ready;
};

static Sdl3MixerAmbientBus g_ambient_bus;

extern "C" {

void sdl3_mixer_ambient_bus_init(int output_rate, int channels)
{
	sdl3_mixer_ambient_bus_shutdown();

	g_ambient_bus.output_rate = (output_rate > 0) ? output_rate : 48000;
	g_ambient_bus.channels = (channels > 0) ? channels : 2;
	g_ambient_bus.ready = true;
}

void sdl3_mixer_ambient_bus_shutdown(void)
{
	memset(&g_ambient_bus, 0, sizeof(g_ambient_bus));
}

bool sdl3_mixer_ambient_bus_ready(void)
{
	return g_ambient_bus.ready;
}

void sdl3_mixer_ambient_bus_render(float *mix_l, float *mix_r, int frames, int output_rate)
{
	if (!g_ambient_bus.ready) {
		return;
	}
	sdl3_mix_ambient_voices(mix_l, mix_r, frames, output_rate);
}

} /* extern "C" */
