/*
** Music bus owned by sdl3_mixer — PCM/render wiring is added later.
*/
#ifndef AUDIO_SDL3_MIXER_MUSIC_BUS_H
#define AUDIO_SDL3_MIXER_MUSIC_BUS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void sdl3_mixer_music_bus_init(int output_rate, int channels);
void sdl3_mixer_music_bus_shutdown(void);

bool sdl3_mixer_music_bus_ready(void);
int sdl3_mixer_music_bus_output_rate(void);
int sdl3_mixer_music_bus_channels(void);

void sdl3_mixer_music_bus_lock(void);
void sdl3_mixer_music_bus_unlock(void);

/* Sum into mix buffers; silent until sources are wired in. */
void sdl3_mixer_music_bus_render(float *mix_l, float *mix_r, int frames, int output_rate);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_SDL3_MIXER_MUSIC_BUS_H */
