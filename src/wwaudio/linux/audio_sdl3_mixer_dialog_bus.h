/*
** Dialog / EVA voice bus (TYPE_DIALOG, in-game speech).
*/
#ifndef AUDIO_SDL3_MIXER_DIALOG_BUS_H
#define AUDIO_SDL3_MIXER_DIALOG_BUS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void sdl3_mixer_dialog_bus_init(int output_rate, int channels);
void sdl3_mixer_dialog_bus_shutdown(void);

bool sdl3_mixer_dialog_bus_ready(void);

void sdl3_mixer_dialog_bus_render(float *mix_l, float *mix_r, int frames, int output_rate);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_SDL3_MIXER_DIALOG_BUS_H */
