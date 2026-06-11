/*
** Schroeder-style reverb with EAX 1 preset table (MSS / OpenAL-soft EAX1 values).
 */

#include "eax_reverb.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
	kNumCombs = 4,
	kNumAllpass = 2
};

static const int kCombTuning[kNumCombs] = {1116, 1188, 1277, 1356};
static const int kAllpassTuning[kNumAllpass] = {556, 441};
static const float kAllpassFeedback = 0.5f;
static const float kCombFeedbackBase = 0.84f;

static const Eax1Preset kEax1Presets[EAX_ENVIRONMENT_COUNT] = {
	{0.5f, 1.493f, 0.5f},      /* GENERIC */
	{0.25f, 0.1f, 0.0f},      /* PADDEDCELL */
	{0.417f, 0.4f, 0.666f},   /* ROOM */
	{0.653f, 1.499f, 0.166f}, /* BATHROOM */
	{0.208f, 0.478f, 0.0f},   /* LIVINGROOM */
	{0.5f, 2.309f, 0.888f},   /* STONEROOM */
	{0.403f, 4.279f, 0.5f},   /* AUDITORIUM */
	{0.5f, 3.961f, 0.5f},     /* CONCERTHALL */
	{0.5f, 2.886f, 1.304f},  /* CAVE */
	{0.361f, 7.284f, 0.332f}, /* ARENA */
	{0.5f, 10.0f, 0.3f},      /* HANGAR */
	{0.153f, 0.259f, 2.0f},   /* CARPETEDHALLWAY */
	{0.361f, 1.493f, 0.0f},   /* HALLWAY */
	{0.444f, 2.697f, 0.638f}, /* STONECORRIDOR */
	{0.25f, 1.752f, 0.776f},  /* ALLEY */
	{0.111f, 3.145f, 0.472f}, /* FOREST */
	{0.111f, 2.767f, 0.224f}, /* CITY */
	{0.194f, 7.841f, 0.472f}, /* MOUNTAINS */
	{1.0f, 1.499f, 0.5f},     /* QUARRY */
	{0.097f, 2.767f, 0.224f}, /* PLAIN */
	{0.208f, 1.652f, 1.5f},   /* PARKINGLOT */
	{0.652f, 2.886f, 0.25f},  /* SEWERPIPE */
	{1.0f, 1.499f, 0.0f},     /* UNDERWATER */
	{0.875f, 8.392f, 1.388f}, /* DRUGGED */
	{0.139f, 17.234f, 0.666f}, /* DIZZY */
	{0.486f, 7.563f, 0.806f}, /* PSYCHOTIC */
};

struct Comb {
	int size;
	int idx;
	float *buf;
	float feedback;
	float damp1;
	float damp2;
	float store;
};

struct Allpass {
	int size;
	int idx;
	float *buf;
};

struct EaxReverb {
	int sample_rate;
	float volume;
	float decay_sec;
	float damping;
	float predelay_sec;
	float comb_tune[kNumCombs];
	float comb_fb[kNumCombs];
	Comb comb[kNumCombs];
	Allpass allpass[kNumAllpass];
	float *predelay_buf;
	int predelay_size;
	int predelay_idx;
	float predelay_fill;
};

static int scaled_delay(int base_samples, int sample_rate)
{
	int n = (base_samples * sample_rate) / 44100;

	if (n < 1) {
		n = 1;
	}
	return n;
}

static void comb_init(Comb *c, int size, float feedback, float damping)
{
	c->size = size;
	c->idx = 0;
	c->buf = (float *)calloc((size_t)size, sizeof(float));
	c->feedback = feedback;
	c->damp1 = damping;
	c->damp2 = 1.0f - damping;
	c->store = 0.0f;
}

static void allpass_init(Allpass *a, int size)
{
	a->size = size;
	a->idx = 0;
	a->buf = (float *)calloc((size_t)size, sizeof(float));
}

static void comb_free(Comb *c)
{
	free(c->buf);
	c->buf = NULL;
}

static void allpass_free(Allpass *a)
{
	free(a->buf);
	a->buf = NULL;
}

static float comb_process(Comb *c, float input)
{
	float output;
	float bufout;

	if (c->buf == NULL || c->size <= 0) {
		return 0.0f;
	}

	output = c->buf[c->idx];
	c->store = (output * c->damp1) + (c->store * c->damp2);
	bufout = input + (c->store * c->feedback);
	c->buf[c->idx] = bufout;
	c->idx++;
	if (c->idx >= c->size) {
		c->idx = 0;
	}
	return output;
}

static float allpass_process(Allpass *a, float input)
{
	float bufout;
	float output;

	if (a->buf == NULL || a->size <= 0) {
		return input;
	}

	bufout = a->buf[a->idx];
	output = (-input) + bufout;
	a->buf[a->idx] = input + (bufout * kAllpassFeedback);
	a->idx++;
	if (a->idx >= a->size) {
		a->idx = 0;
	}
	return output;
}

static void eax_reverb_apply_tuning(EaxReverb *rev)
{
	int i;
	float decay;

	if (rev == NULL) {
		return;
	}

	decay = rev->decay_sec;
	if (decay < 0.1f) {
		decay = 0.1f;
	}

	for (i = 0; i < kNumCombs; i++) {
		int size = scaled_delay(kCombTuning[i], rev->sample_rate);
		float fb;

		rev->comb_tune[i] = (float)size;
		fb = kCombFeedbackBase * (1.0f - (0.3f / decay));
		if (fb > 0.98f) {
			fb = 0.98f;
		}
		if (fb < 0.5f) {
			fb = 0.5f;
		}
		rev->comb_fb[i] = fb;
		if (rev->comb[i].size != size) {
			comb_free(&rev->comb[i]);
			comb_init(&rev->comb[i], size, fb, rev->damping);
		} else {
			rev->comb[i].feedback = fb;
			rev->comb[i].damp1 = rev->damping;
			rev->comb[i].damp2 = 1.0f - rev->damping;
		}
	}

	for (i = 0; i < kNumAllpass; i++) {
		int size = scaled_delay(kAllpassTuning[i], rev->sample_rate);

		if (rev->allpass[i].size != size) {
			allpass_free(&rev->allpass[i]);
			allpass_init(&rev->allpass[i], size);
		}
	}

	free(rev->predelay_buf);
	rev->predelay_size = (int)(rev->predelay_sec * (float)rev->sample_rate);
	if (rev->predelay_size < 1) {
		rev->predelay_size = 0;
		rev->predelay_buf = NULL;
	} else {
		rev->predelay_buf = (float *)calloc((size_t)rev->predelay_size, sizeof(float));
	}
	rev->predelay_idx = 0;
	rev->predelay_fill = 0.0f;
}

static int g_listener_room = 0;

extern "C" const Eax1Preset *eax_reverb_get_preset(int environment)
{
	if (environment < 0 || environment >= EAX_ENVIRONMENT_COUNT) {
		environment = 0;
	}
	return &kEax1Presets[environment];
}

extern "C" void eax_set_listener_room(int environment)
{
	if (environment < 0 || environment >= EAX_ENVIRONMENT_COUNT) {
		environment = 0;
	}
	g_listener_room = environment;
}

extern "C" int eax_get_listener_room(void)
{
	return g_listener_room;
}

extern "C" EaxReverb *eax_reverb_create(int sample_rate)
{
	EaxReverb *rev;
	int i;

	if (sample_rate <= 0) {
		sample_rate = 44100;
	}

	rev = (EaxReverb *)calloc(1, sizeof(EaxReverb));
	if (rev == NULL) {
		return NULL;
	}

	rev->sample_rate = sample_rate;
	rev->volume = 0.5f;
	rev->decay_sec = 1.493f;
	rev->damping = 0.5f;
	rev->predelay_sec = 0.0f;

	for (i = 0; i < kNumCombs; i++) {
		comb_init(&rev->comb[i], 1, kCombFeedbackBase, rev->damping);
	}
	for (i = 0; i < kNumAllpass; i++) {
		allpass_init(&rev->allpass[i], 1);
	}

	eax_reverb_set_preset(rev, g_listener_room);
	return rev;
}

extern "C" void eax_reverb_destroy(EaxReverb *rev)
{
	int i;

	if (rev == NULL) {
		return;
	}

	for (i = 0; i < kNumCombs; i++) {
		comb_free(&rev->comb[i]);
	}
	for (i = 0; i < kNumAllpass; i++) {
		allpass_free(&rev->allpass[i]);
	}
	free(rev->predelay_buf);
	free(rev);
}

extern "C" void eax_reverb_set_preset(EaxReverb *rev, int environment)
{
	const Eax1Preset *p;

	if (rev == NULL) {
		return;
	}

	p = eax_reverb_get_preset(environment);
	rev->volume = p->volume;
	rev->decay_sec = p->decay_sec;
	rev->damping = p->damping;
	rev->predelay_sec = 0.0f;
	eax_reverb_apply_tuning(rev);
}

extern "C" void eax_reverb_set_custom(EaxReverb *rev, float volume, float decay_sec, float damping, float predelay_sec)
{
	if (rev == NULL) {
		return;
	}

	rev->volume = volume;
	rev->decay_sec = decay_sec;
	if (rev->decay_sec < 0.1f) {
		rev->decay_sec = 0.1f;
	}
	rev->damping = damping;
	if (rev->damping < 0.0f) {
		rev->damping = 0.0f;
	}
	if (rev->damping > 1.0f) {
		rev->damping = 1.0f;
	}
	rev->predelay_sec = predelay_sec;
	if (rev->predelay_sec < 0.0f) {
		rev->predelay_sec = 0.0f;
	}
	eax_reverb_apply_tuning(rev);
}

extern "C" void eax_reverb_clear(EaxReverb *rev)
{
	int i;

	if (rev == NULL) {
		return;
	}

	for (i = 0; i < kNumCombs; i++) {
		if (rev->comb[i].buf != NULL) {
			memset(rev->comb[i].buf, 0, (size_t)rev->comb[i].size * sizeof(float));
		}
		rev->comb[i].idx = 0;
		rev->comb[i].store = 0.0f;
	}
	for (i = 0; i < kNumAllpass; i++) {
		if (rev->allpass[i].buf != NULL) {
			memset(rev->allpass[i].buf, 0, (size_t)rev->allpass[i].size * sizeof(float));
		}
		rev->allpass[i].idx = 0;
	}
	if (rev->predelay_buf != NULL) {
		memset(rev->predelay_buf, 0, (size_t)rev->predelay_size * sizeof(float));
	}
	rev->predelay_idx = 0;
	rev->predelay_fill = 0.0f;
}

extern "C" void eax_reverb_process(EaxReverb *rev, float mono_in, float wet_gain, float *out_l, float *out_r)
{
	float input;
	float sum;
	float mono;
	int i;

	if (out_l == NULL || out_r == NULL) {
		return;
	}

	*out_l = 0.0f;
	*out_r = 0.0f;

	if (rev == NULL || wet_gain <= 0.0001f) {
		return;
	}

	input = mono_in;

	if (rev->predelay_size > 0 && rev->predelay_buf != NULL) {
		float delayed = rev->predelay_buf[rev->predelay_idx];
		rev->predelay_buf[rev->predelay_idx] = input;
		rev->predelay_idx++;
		if (rev->predelay_idx >= rev->predelay_size) {
			rev->predelay_idx = 0;
		}
		input = delayed;
	}

	sum = 0.0f;
	for (i = 0; i < kNumCombs; i++) {
		sum += comb_process(&rev->comb[i], input);
	}
	sum *= 0.25f;

	for (i = 0; i < kNumAllpass; i++) {
		sum = allpass_process(&rev->allpass[i], sum);
	}

	mono = sum * rev->volume * wet_gain;
	*out_l = mono;
	*out_r = mono;
}
