/*
** Voice mixing for sdl3_mixer buses (game thread).
*/
#ifndef SDL3_MIX_EXPORT_H
#define SDL3_MIX_EXPORT_H

#include "mss_stub.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDL3_VOICE_BUS_WEAPON  0
#define SDL3_VOICE_BUS_UI      1
#define SDL3_VOICE_BUS_AMBIENT 2
#define SDL3_VOICE_BUS_DIALOG  3

void sdl3_mix_weapon_voices(float *mix_l, float *mix_r, int frames, int output_rate);
void sdl3_mix_ambient_voices(float *mix_l, float *mix_r, int frames, int output_rate);
void sdl3_mix_dialog_voices(float *mix_l, float *mix_r, int frames, int output_rate);
void sdl3_mix_ui_voices(float *mix_l, float *mix_r, int frames, int output_rate);

/* bus = SDL3_VOICE_BUS_* or -1 to apply default routing. */
void sdl3_voice_set_bus(HSAMPLE sample, int bus);

/*
** Legacy UI toggle: ui_bus=1 → UI, ui_bus=0 → weapon, ui_bus=-1 → default.
*/
void sdl3_voice_set_ui_bus(HSAMPLE sample, int ui_bus);

#ifdef __cplusplus
}
#endif

#endif /* SDL3_MIX_EXPORT_H */
