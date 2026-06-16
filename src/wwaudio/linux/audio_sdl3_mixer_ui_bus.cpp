/*
** UI bus: menu clicks and hover (runtime priority >= 100).
*/

#include "audio_sdl3_mixer_ui_bus.h"
#include "sdl3_mix_export.h"

#include <SDL3/SDL.h>
#include <string.h>

struct Sdl3MixerUiBus
{
	int output_rate;
	int channels;
	bool ready;
};

static Sdl3MixerUiBus g_ui_bus;

extern "C" {

void sdl3_mixer_ui_bus_init(int output_rate, int channels)
{
	sdl3_mixer_ui_bus_shutdown();

	g_ui_bus.output_rate = (output_rate > 0) ? output_rate : 48000;
	g_ui_bus.channels = (channels > 0) ? channels : 2;
	g_ui_bus.ready = true;
}

void sdl3_mixer_ui_bus_shutdown(void)
{
	memset(&g_ui_bus, 0, sizeof(g_ui_bus));
}

bool sdl3_mixer_ui_bus_ready(void)
{
	return g_ui_bus.ready;
}

void sdl3_mixer_ui_bus_render(float *mix_l, float *mix_r, int frames, int output_rate)
{
	if (!g_ui_bus.ready) {
		return;
	}
	sdl3_mix_ui_voices(mix_l, mix_r, frames, output_rate);
}

} /* extern "C" */
