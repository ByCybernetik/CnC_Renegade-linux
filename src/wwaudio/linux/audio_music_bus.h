/*
** Dedicated background-music bus for the SDL3 audio backend.
** Music lives outside the Miles/HSAMPLE voice pool and cannot be stolen by SFX.
*/
#ifndef AUDIO_MUSIC_BUS_H
#define AUDIO_MUSIC_BUS_H

#include <SDL3/SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void music_bus_attach(SDL_AudioDeviceID device, const SDL_AudioSpec *device_spec);
void music_bus_detach(void);

bool music_bus_play(const void *file_data, unsigned long file_len, float volume, int fade_in_ms);
void music_bus_stop(int fade_out_ms);
void music_bus_set_volume(float volume);
void music_bus_pause(bool paused);
void music_bus_resume_if_paused(void);
bool music_bus_is_playing(void);

/* Called from sdl3_mixer music bus callback on the game thread. */
void music_bus_render_mix(float *mix_l, float *mix_r, int frames, int output_rate);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_MUSIC_BUS_H */
