/*
** SDL3-native software mixer — game thread mixes, SDL_AudioStream consumes via Put.
** Standalone module; buses: music → ambient → weapon → dialog → UI.
*/
#ifndef AUDIO_SDL3_MIXER_H
#define AUDIO_SDL3_MIXER_H

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*Sdl3MixerBusFn)(float *mix_l, float *mix_r, int frames, int output_rate, void *userdata);

void sdl3_mixer_init(SDL_AudioDeviceID device, const SDL_AudioSpec *device_spec);
void sdl3_mixer_shutdown(void);

int sdl3_mixer_output_rate(void);
int sdl3_mixer_channels(void);

void sdl3_mixer_set_music_bus(Sdl3MixerBusFn fn, void *userdata);
void sdl3_mixer_set_ambient_bus(Sdl3MixerBusFn fn, void *userdata);
void sdl3_mixer_set_sfx_bus(Sdl3MixerBusFn fn, void *userdata);
void sdl3_mixer_set_dialog_bus(Sdl3MixerBusFn fn, void *userdata);
void sdl3_mixer_set_ui_bus(Sdl3MixerBusFn fn, void *userdata);

void sdl3_mixer_lock(void);
void sdl3_mixer_unlock(void);

/* Mix and SDL_PutAudioStreamData; keeps ~target queue depth. milliseconds = frame delta (0 = auto). */
void sdl3_mixer_pump(unsigned int milliseconds);

/* Stop feeding the device while menus pause game audio (stream queue cleared on resume). */
void sdl3_mixer_set_game_paused(bool paused);
bool sdl3_mixer_is_game_paused(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_SDL3_MIXER_H */
