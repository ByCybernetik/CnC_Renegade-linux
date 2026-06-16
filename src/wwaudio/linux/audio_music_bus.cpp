/*
** Background-music bus — PCM state only; mixer wiring is separate.
*/

#include "audio_music_bus.h"
#include "audio_sdl3_mixer.h"
#include "audio_decode.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct MusicBus
{
	int output_rate;
	unsigned char *pcm;
	unsigned long pcm_bytes;
	volatile double pcm_frame_pos;
	int rate;
	int channels;
	SDL_Mutex *lock;

	volatile float user_gain;
	volatile bool paused;
	volatile bool halted;

	float fade_start_value;
	float fade_target;
	Uint64 fade_start_ms;
	int fade_duration_ms;
	volatile bool fade_active;
	volatile bool stop_when_silent;
};

static MusicBus g_bus;
static bool g_attached = false;

static void music_bus_zero(MusicBus *bus)
{
	if (bus == NULL) {
		return;
	}
	memset(bus, 0, sizeof(*bus));
	bus->user_gain = 1.0f;
}

static int music_bus_upmix_mono(unsigned char **pcm, unsigned long *pcm_bytes)
{
	unsigned long sample_count;
	unsigned char *out;
	unsigned short *src;
	unsigned short *dst;
	unsigned long i;

	if (pcm == NULL || pcm_bytes == NULL || *pcm == NULL || *pcm_bytes < 2) {
		return 0;
	}

	sample_count = *pcm_bytes / 2;
	out = (unsigned char *)malloc(*pcm_bytes * 2);
	if (out == NULL) {
		return 0;
	}

	src = (unsigned short *)*pcm;
	dst = (unsigned short *)out;
	for (i = 0; i < sample_count; i++) {
		dst[i * 2] = src[i];
		dst[i * 2 + 1] = src[i];
	}

	free(*pcm);
	*pcm = out;
	*pcm_bytes *= 2;
	return 1;
}

static void music_bus_clear_pcm_locked(MusicBus *bus)
{
	if (bus == NULL) {
		return;
	}

	audio_decode_free(bus->pcm);
	bus->pcm = NULL;
	bus->pcm_bytes = 0;
	bus->pcm_frame_pos = 0.0;
	bus->halted = true;
	bus->fade_active = false;
	bus->stop_when_silent = false;
}

static float music_bus_effective_gain(const MusicBus *bus)
{
	float gain;

	if (bus == NULL) {
		return 0.0f;
	}

	gain = bus->user_gain;
	if (!bus->fade_active) {
		return gain;
	}

	if (bus->fade_duration_ms <= 0) {
		gain *= bus->fade_target;
		return gain;
	}

	Uint64 elapsed = (Uint64)SDL_GetTicks() - bus->fade_start_ms;
	if (elapsed >= (Uint64)bus->fade_duration_ms) {
		gain *= bus->fade_target;
		return gain;
	}

	{
		float t = (float)elapsed / (float)bus->fade_duration_ms;
		float fade = bus->fade_start_value + (bus->fade_target - bus->fade_start_value) * t;
		gain *= fade;
	}
	return gain;
}

static void music_bus_begin_fade(MusicBus *bus, float target, int duration_ms)
{
	float current;

	if (bus == NULL) {
		return;
	}

	current = music_bus_effective_gain(bus) / ((bus->user_gain > 0.0001f) ? bus->user_gain : 1.0f);
	bus->fade_start_value = current;
	bus->fade_target = target;
	bus->fade_start_ms = (Uint64)SDL_GetTicks();
	bus->fade_duration_ms = duration_ms;
	bus->fade_active = true;
}

extern "C" {

void music_bus_render_mix(float *mix_l, float *mix_r, int frames, int output_rate)
{
	double src_step;
	double pos;
	unsigned long frame_count;
	float gain;
	int i;

	if (mix_l == NULL || mix_r == NULL || frames <= 0 || output_rate <= 0) {
		return;
	}
	if (g_bus.pcm == NULL || g_bus.pcm_bytes < 4 || g_bus.halted) {
		return;
	}

	gain = music_bus_effective_gain(&g_bus);
	if (gain <= 0.0001f) {
		if (g_bus.fade_active) {
			Uint64 elapsed = (Uint64)SDL_GetTicks() - g_bus.fade_start_ms;
			if (g_bus.fade_duration_ms <= 0 || elapsed >= (Uint64)g_bus.fade_duration_ms) {
				g_bus.fade_active = false;
				if (g_bus.stop_when_silent) {
					g_bus.halted = true;
				}
			}
		}
		return;
	}

	frame_count = g_bus.pcm_bytes / 4;
	if (frame_count == 0) {
		return;
	}

	src_step = (g_bus.rate > 0) ? ((double)g_bus.rate / (double)output_rate) : 1.0;
	pos = g_bus.pcm_frame_pos;

	if (g_bus.rate == output_rate) {
		for (i = 0; i < frames; i++) {
			unsigned long idx;
			short sample_l;
			short sample_r;

			if (g_bus.paused) {
				continue;
			}

			idx = (unsigned long)fmod(pos, (double)frame_count);
			sample_l = *(const short *)(g_bus.pcm + idx * 4);
			sample_r = *(const short *)(g_bus.pcm + idx * 4 + 2);
			mix_l[i] += (float)sample_l * gain;
			mix_r[i] += (float)sample_r * gain;
			pos += 1.0;
		}
		if (!g_bus.paused) {
			g_bus.pcm_frame_pos = fmod(pos, (double)frame_count);
		}
	} else {
		for (i = 0; i < frames; i++) {
			unsigned long i0;
			unsigned long i1;
			float frac;
			short l0;
			short r0;
			short l1;
			short r1;
			float s_l;
			float s_r;

			if (g_bus.paused) {
				continue;
			}

			i0 = (unsigned long)fmod(pos, (double)frame_count);
			i1 = (i0 + 1) % frame_count;
			frac = (float)(pos - floor(pos));
			l0 = *(const short *)(g_bus.pcm + i0 * 4);
			r0 = *(const short *)(g_bus.pcm + i0 * 4 + 2);
			l1 = *(const short *)(g_bus.pcm + i1 * 4);
			r1 = *(const short *)(g_bus.pcm + i1 * 4 + 2);
			s_l = ((float)l0 + ((float)l1 - (float)l0) * frac) * gain;
			s_r = ((float)r0 + ((float)r1 - (float)r0) * frac) * gain;
			mix_l[i] += s_l;
			mix_r[i] += s_r;
			pos += src_step;
		}

		if (!g_bus.paused) {
			g_bus.pcm_frame_pos = pos;
		}
	}

	if (g_bus.fade_active) {
		Uint64 elapsed = (Uint64)SDL_GetTicks() - g_bus.fade_start_ms;
		if (g_bus.fade_duration_ms <= 0 || elapsed >= (Uint64)g_bus.fade_duration_ms) {
			g_bus.fade_active = false;
			if (g_bus.stop_when_silent && g_bus.fade_target <= 0.0f) {
				g_bus.halted = true;
			}
		}
	}
}

void music_bus_attach(SDL_AudioDeviceID device, const SDL_AudioSpec *device_spec)
{
	(void)device;

	if (g_bus.lock == NULL) {
		g_bus.lock = SDL_CreateMutex();
	}

	SDL_LockMutex(g_bus.lock);
	sdl3_mixer_lock();
	music_bus_clear_pcm_locked(&g_bus);
	g_bus.output_rate = (device_spec != NULL && device_spec->freq > 0) ? device_spec->freq : 48000;
	g_bus.user_gain = 1.0f;
	g_attached = true;
	sdl3_mixer_unlock();
	SDL_UnlockMutex(g_bus.lock);
}

void music_bus_detach(void)
{
	if (g_bus.lock == NULL) {
		return;
	}

	SDL_LockMutex(g_bus.lock);
	sdl3_mixer_lock();
	music_bus_clear_pcm_locked(&g_bus);
	g_attached = false;
	sdl3_mixer_unlock();
	SDL_UnlockMutex(g_bus.lock);

	SDL_DestroyMutex(g_bus.lock);
	g_bus.lock = NULL;
	music_bus_zero(&g_bus);
}

bool music_bus_play(const void *file_data, unsigned long file_len, float volume, int fade_in_ms)
{
	AILSOUNDINFO info;
	unsigned char *pcm = NULL;
	unsigned long pcm_bytes = 0;
	unsigned char *new_pcm = NULL;
	unsigned long new_bytes = 0;
	int new_rate;
	int new_channels;

	if (!g_attached || g_bus.lock == NULL) {
		return false;
	}
	if (file_data == NULL || file_len == 0) {
		return false;
	}

	if (!audio_decode_to_pcm(file_data, file_len, &pcm, &pcm_bytes, &info)) {
		return false;
	}

	new_pcm = pcm;
	new_bytes = pcm_bytes;
	new_rate = info.rate;
	new_channels = (info.channels >= 2) ? 2 : 1;
	if (new_channels == 2 && new_bytes >= 4) {
		new_bytes &= ~(unsigned long)3;
	}
	if (new_channels == 1) {
		if (!music_bus_upmix_mono(&new_pcm, &new_bytes)) {
			audio_decode_free(pcm);
			return false;
		}
		new_channels = 2;
	}

	if (g_bus.output_rate > 0) {
		if (!audio_pcm_resample_to_rate(&new_pcm, &new_bytes, &new_rate, new_channels, g_bus.output_rate)) {
			audio_decode_free(new_pcm);
			return false;
		}
	}

	SDL_LockMutex(g_bus.lock);
	sdl3_mixer_lock();
	music_bus_clear_pcm_locked(&g_bus);
	g_bus.pcm = new_pcm;
	g_bus.pcm_bytes = new_bytes;
	g_bus.rate = new_rate;
	g_bus.channels = new_channels;
	g_bus.pcm_frame_pos = 0.0;
	g_bus.user_gain = volume;
	g_bus.paused = false;
	g_bus.halted = false;
	g_bus.stop_when_silent = false;

	if (fade_in_ms > 0) {
		g_bus.fade_start_value = 0.0f;
		g_bus.fade_target = 1.0f;
		g_bus.fade_start_ms = (Uint64)SDL_GetTicks();
		g_bus.fade_duration_ms = fade_in_ms;
		g_bus.fade_active = true;
	} else {
		g_bus.fade_active = false;
	}

	sdl3_mixer_unlock();
	SDL_UnlockMutex(g_bus.lock);
	return true;
}

void music_bus_stop(int fade_out_ms)
{
	if (g_bus.lock == NULL) {
		return;
	}

	SDL_LockMutex(g_bus.lock);

	if (g_bus.pcm == NULL) {
		sdl3_mixer_lock();
		music_bus_clear_pcm_locked(&g_bus);
		sdl3_mixer_unlock();
		SDL_UnlockMutex(g_bus.lock);
		return;
	}

	if (fade_out_ms > 0) {
		g_bus.stop_when_silent = true;
		music_bus_begin_fade(&g_bus, 0.0f, fade_out_ms);
	} else {
		sdl3_mixer_lock();
		music_bus_clear_pcm_locked(&g_bus);
		sdl3_mixer_unlock();
	}

	SDL_UnlockMutex(g_bus.lock);
}

void music_bus_set_volume(float volume)
{
	if (g_bus.lock == NULL) {
		return;
	}

	if (volume < 0.0f) {
		volume = 0.0f;
	}
	if (volume > 1.0f) {
		volume = 1.0f;
	}

	SDL_LockMutex(g_bus.lock);
	g_bus.user_gain = volume;
	SDL_UnlockMutex(g_bus.lock);
}

void music_bus_pause(bool paused)
{
	if (g_bus.lock == NULL) {
		return;
	}

	SDL_LockMutex(g_bus.lock);
	g_bus.paused = paused;
	SDL_UnlockMutex(g_bus.lock);
}

void music_bus_resume_if_paused(void)
{
	if (g_bus.lock == NULL) {
		return;
	}

	SDL_LockMutex(g_bus.lock);
	if (g_bus.paused) {
		SDL_UnlockMutex(g_bus.lock);
		music_bus_pause(false);
	} else {
		SDL_UnlockMutex(g_bus.lock);
	}
}

bool music_bus_is_playing(void)
{
	bool playing;

	if (g_bus.lock == NULL) {
		return false;
	}

	SDL_LockMutex(g_bus.lock);
	playing = (g_bus.pcm != NULL && !g_bus.halted);
	SDL_UnlockMutex(g_bus.lock);
	return playing;
}

} /* extern "C" */
