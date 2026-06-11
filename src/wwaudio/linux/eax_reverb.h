/*
** EAX 1-style algorithmic reverb for the SDL3 Miles shim.
** Presets match Creative EAX 1 / MSS eax.h (volume, decay, damping).
 */
#ifndef EAX_REVERB_H
#define EAX_REVERB_H

#ifdef __cplusplus
extern "C" {
#endif

#define EAX_ENVIRONMENT_COUNT 26

typedef struct Eax1Preset {
	float volume;
	float decay_sec;
	float damping;
} Eax1Preset;

typedef struct EaxReverb EaxReverb;

EaxReverb *eax_reverb_create(int sample_rate);
void eax_reverb_destroy(EaxReverb *rev);

void eax_reverb_set_preset(EaxReverb *rev, int environment);
void eax_reverb_set_custom(EaxReverb *rev, float volume, float decay_sec, float damping, float predelay_sec);

void eax_reverb_process(EaxReverb *rev, float mono_in, float wet_gain, float *out_l, float *out_r);
void eax_reverb_clear(EaxReverb *rev);

const Eax1Preset *eax_reverb_get_preset(int environment);

void eax_set_listener_room(int environment);
int eax_get_listener_room(void);

#ifdef __cplusplus
}
#endif

#endif
