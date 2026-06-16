/*
** Dedicated music bus for sdl3_mixer.
*/

#include "audio_sdl3_mixer_music_bus.h"
#include "audio_music_bus.h"

#include <SDL3/SDL.h>
#include <string.h>

struct Sdl3MixerMusicBus
{
	int output_rate;
	int channels;
	SDL_Mutex *lock;
	bool ready;
};

static Sdl3MixerMusicBus g_music_bus;

extern "C" {

void sdl3_mixer_music_bus_init(int output_rate, int channels)
{
	sdl3_mixer_music_bus_shutdown();

	g_music_bus.output_rate = (output_rate > 0) ? output_rate : 48000;
	g_music_bus.channels = (channels > 0) ? channels : 2;
	g_music_bus.lock = SDL_CreateMutex();
	g_music_bus.ready = (g_music_bus.lock != NULL);
}

void sdl3_mixer_music_bus_shutdown(void)
{
	if (g_music_bus.lock != NULL) {
		SDL_DestroyMutex(g_music_bus.lock);
		g_music_bus.lock = NULL;
	}

	memset(&g_music_bus, 0, sizeof(g_music_bus));
}

bool sdl3_mixer_music_bus_ready(void)
{
	return g_music_bus.ready;
}

int sdl3_mixer_music_bus_output_rate(void)
{
	return g_music_bus.output_rate;
}

int sdl3_mixer_music_bus_channels(void)
{
	return g_music_bus.channels;
}

void sdl3_mixer_music_bus_lock(void)
{
	if (g_music_bus.lock != NULL) {
		SDL_LockMutex(g_music_bus.lock);
	}
}

void sdl3_mixer_music_bus_unlock(void)
{
	if (g_music_bus.lock != NULL) {
		SDL_UnlockMutex(g_music_bus.lock);
	}
}

void sdl3_mixer_music_bus_render(float *mix_l, float *mix_r, int frames, int output_rate)
{
	if (!g_music_bus.ready) {
		return;
	}
	music_bus_render_mix(mix_l, mix_r, frames, output_rate);
}

} /* extern "C" */
