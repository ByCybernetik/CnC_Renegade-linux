/*
** Ambient / environment bus (wind, rain, level beds, static world loops).
*/
#ifndef AUDIO_SDL3_MIXER_AMBIENT_BUS_H
#define AUDIO_SDL3_MIXER_AMBIENT_BUS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void sdl3_mixer_ambient_bus_init(int output_rate, int channels);
void sdl3_mixer_ambient_bus_shutdown(void);

bool sdl3_mixer_ambient_bus_ready(void);

void sdl3_mixer_ambient_bus_render(float *mix_l, float *mix_r, int frames, int output_rate);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_SDL3_MIXER_AMBIENT_BUS_H */
