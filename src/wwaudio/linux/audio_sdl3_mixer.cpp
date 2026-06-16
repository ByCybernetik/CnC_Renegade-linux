/*
** SDL3 audio output: one bound stream, no GetCallback / ring buffer.
** Each pump() mixes a frame window and queues PCM with SDL_PutAudioStreamData.
*/

#include "audio_sdl3_mixer.h"
#include "audio_sdl3_mixer_music_bus.h"
#include "audio_sdl3_mixer_ui_bus.h"
#include "audio_sdl3_mixer_weapon_bus.h"
#include "audio_sdl3_mixer_ambient_bus.h"
#include "audio_sdl3_mixer_dialog_bus.h"
#include "audio_music_bus.h"

#include <string.h>

#define SDL3_MIXER_CHUNK_FRAMES 512
#define SDL3_MIXER_MIN_QUEUE_MS 25
#define SDL3_MIXER_TARGET_QUEUE_MS 50
#define SDL3_MIXER_MAX_QUEUE_MS 100

struct Sdl3Mixer
{
	SDL_AudioDeviceID device;
	SDL_AudioStream *stream;
	SDL_AudioSpec device_spec;
	int output_rate;
	int channels;
	int bytes_per_frame;

	Uint64 last_pump_ticks;
	bool game_paused;

	Sdl3MixerBusFn music_fn;
	void *music_ud;
	Sdl3MixerBusFn ambient_fn;
	void *ambient_ud;
	Sdl3MixerBusFn sfx_fn;
	void *sfx_ud;
	Sdl3MixerBusFn dialog_fn;
	void *dialog_ud;
	Sdl3MixerBusFn ui_fn;
	void *ui_ud;
};

static Sdl3Mixer g_mx;

static void sdl3_mixer_render_chunk(int frames, unsigned char *out_bytes, int *out_len)
{
	float mix_l[SDL3_MIXER_CHUNK_FRAMES];
	float mix_r[SDL3_MIXER_CHUNK_FRAMES];
	int i;

	if (out_bytes == NULL || out_len == NULL || frames <= 0 || frames > SDL3_MIXER_CHUNK_FRAMES) {
		if (out_len != NULL) {
			*out_len = 0;
		}
		return;
	}

	memset(mix_l, 0, (size_t)frames * sizeof(float));
	memset(mix_r, 0, (size_t)frames * sizeof(float));

	if (g_mx.music_fn != NULL) {
		g_mx.music_fn(mix_l, mix_r, frames, g_mx.output_rate, g_mx.music_ud);
	}
	if (g_mx.ambient_fn != NULL) {
		g_mx.ambient_fn(mix_l, mix_r, frames, g_mx.output_rate, g_mx.ambient_ud);
	}
	if (g_mx.sfx_fn != NULL) {
		g_mx.sfx_fn(mix_l, mix_r, frames, g_mx.output_rate, g_mx.sfx_ud);
	}
	if (g_mx.dialog_fn != NULL) {
		g_mx.dialog_fn(mix_l, mix_r, frames, g_mx.output_rate, g_mx.dialog_ud);
	}
	if (g_mx.ui_fn != NULL) {
		g_mx.ui_fn(mix_l, mix_r, frames, g_mx.output_rate, g_mx.ui_ud);
	}

	for (i = 0; i < frames; i++) {
		short *out;
		float dry_l;
		float dry_r;

		dry_l = mix_l[i];
		dry_r = mix_r[i];
		if (dry_l > 32767.0f) {
			dry_l = 32767.0f;
		} else if (dry_l < -32768.0f) {
			dry_l = -32768.0f;
		}
		if (dry_r > 32767.0f) {
			dry_r = 32767.0f;
		} else if (dry_r < -32768.0f) {
			dry_r = -32768.0f;
		}

		out = (short *)(out_bytes + (size_t)i * (size_t)g_mx.bytes_per_frame);
		out[0] = (short)dry_l;
		if (g_mx.channels >= 2) {
			out[1] = (short)dry_r;
		}
	}

	*out_len = frames * g_mx.bytes_per_frame;
}

static int sdl3_mixer_ms_to_bytes(int ms)
{
	if (ms < 0) {
		ms = 0;
	}
	return (g_mx.output_rate * g_mx.bytes_per_frame * ms) / 1000;
}

static int sdl3_mixer_min_queued_bytes(void)
{
	return sdl3_mixer_ms_to_bytes(SDL3_MIXER_MIN_QUEUE_MS);
}

static int sdl3_mixer_target_queued_bytes(void)
{
	return sdl3_mixer_ms_to_bytes(SDL3_MIXER_TARGET_QUEUE_MS);
}

static int sdl3_mixer_max_queued_bytes(void)
{
	return sdl3_mixer_ms_to_bytes(SDL3_MIXER_MAX_QUEUE_MS);
}

static void sdl3_mixer_put_chunk(int frames)
{
	unsigned char chunk[SDL3_MIXER_CHUNK_FRAMES * 4];
	int chunk_bytes;

	if (g_mx.stream == NULL || frames <= 0) {
		return;
	}

	sdl3_mixer_render_chunk(frames, chunk, &chunk_bytes);
	if (chunk_bytes > 0) {
		SDL_PutAudioStreamData(g_mx.stream, chunk, chunk_bytes);
	}
}

extern "C" {

void sdl3_mixer_init(SDL_AudioDeviceID device, const SDL_AudioSpec *device_spec)
{
	sdl3_mixer_shutdown();

	if (device == 0 || device_spec == NULL) {
		return;
	}

	g_mx.device = device;
	g_mx.device_spec = *device_spec;
	g_mx.output_rate = (device_spec->freq > 0) ? device_spec->freq : 48000;
	g_mx.channels = (device_spec->channels > 0) ? device_spec->channels : 2;
	g_mx.bytes_per_frame = (int)sizeof(short) * g_mx.channels;
	g_mx.last_pump_ticks = (Uint64)SDL_GetTicks();
	g_mx.game_paused = false;
	g_mx.music_fn = NULL;
	g_mx.music_ud = NULL;
	g_mx.sfx_fn = NULL;
	g_mx.sfx_ud = NULL;
	g_mx.ui_fn = NULL;
	g_mx.ui_ud = NULL;

	g_mx.stream = SDL_CreateAudioStream(device_spec, device_spec);
	if (g_mx.stream == NULL) {
		return;
	}
	if (!SDL_BindAudioStream(device, g_mx.stream)) {
		SDL_DestroyAudioStream(g_mx.stream);
		g_mx.stream = NULL;
		return;
	}

	SDL_SetAudioStreamGain(g_mx.stream, 1.0f);
	sdl3_mixer_music_bus_init(g_mx.output_rate, g_mx.channels);
	music_bus_attach(device, device_spec);
	sdl3_mixer_set_music_bus(sdl3_mixer_music_bus_render, NULL);
	sdl3_mixer_ambient_bus_init(g_mx.output_rate, g_mx.channels);
	sdl3_mixer_set_ambient_bus(sdl3_mixer_ambient_bus_render, NULL);
	sdl3_mixer_weapon_bus_init(g_mx.output_rate, g_mx.channels);
	sdl3_mixer_set_sfx_bus(sdl3_mixer_weapon_bus_render, NULL);
	sdl3_mixer_dialog_bus_init(g_mx.output_rate, g_mx.channels);
	sdl3_mixer_set_dialog_bus(sdl3_mixer_dialog_bus_render, NULL);
	sdl3_mixer_ui_bus_init(g_mx.output_rate, g_mx.channels);
	sdl3_mixer_set_ui_bus(sdl3_mixer_ui_bus_render, NULL);
}

void sdl3_mixer_shutdown(void)
{
	music_bus_detach();
	sdl3_mixer_set_music_bus(NULL, NULL);
	sdl3_mixer_set_ambient_bus(NULL, NULL);
	sdl3_mixer_set_sfx_bus(NULL, NULL);
	sdl3_mixer_set_dialog_bus(NULL, NULL);
	sdl3_mixer_set_ui_bus(NULL, NULL);
	sdl3_mixer_ui_bus_shutdown();
	sdl3_mixer_dialog_bus_shutdown();
	sdl3_mixer_weapon_bus_shutdown();
	sdl3_mixer_ambient_bus_shutdown();
	sdl3_mixer_music_bus_shutdown();

	if (g_mx.stream != NULL) {
		SDL_ClearAudioStream(g_mx.stream);
		SDL_UnbindAudioStream(g_mx.stream);
		SDL_DestroyAudioStream(g_mx.stream);
		g_mx.stream = NULL;
	}

	g_mx.device = 0;
	g_mx.output_rate = 48000;
	g_mx.channels = 2;
	g_mx.bytes_per_frame = 4;
	g_mx.last_pump_ticks = 0;
	g_mx.game_paused = false;
	g_mx.music_fn = NULL;
	g_mx.music_ud = NULL;
	g_mx.ambient_fn = NULL;
	g_mx.ambient_ud = NULL;
	g_mx.sfx_fn = NULL;
	g_mx.sfx_ud = NULL;
	g_mx.dialog_fn = NULL;
	g_mx.dialog_ud = NULL;
	g_mx.ui_fn = NULL;
	g_mx.ui_ud = NULL;
}

int sdl3_mixer_output_rate(void)
{
	return g_mx.output_rate;
}

int sdl3_mixer_channels(void)
{
	return g_mx.channels;
}

void sdl3_mixer_set_music_bus(Sdl3MixerBusFn fn, void *userdata)
{
	g_mx.music_fn = fn;
	g_mx.music_ud = userdata;
}

void sdl3_mixer_set_ambient_bus(Sdl3MixerBusFn fn, void *userdata)
{
	g_mx.ambient_fn = fn;
	g_mx.ambient_ud = userdata;
}

void sdl3_mixer_set_sfx_bus(Sdl3MixerBusFn fn, void *userdata)
{
	g_mx.sfx_fn = fn;
	g_mx.sfx_ud = userdata;
}

void sdl3_mixer_set_dialog_bus(Sdl3MixerBusFn fn, void *userdata)
{
	g_mx.dialog_fn = fn;
	g_mx.dialog_ud = userdata;
}

void sdl3_mixer_set_ui_bus(Sdl3MixerBusFn fn, void *userdata)
{
	g_mx.ui_fn = fn;
	g_mx.ui_ud = userdata;
}

void sdl3_mixer_lock(void)
{
	if (g_mx.stream != NULL) {
		SDL_LockAudioStream(g_mx.stream);
	}
}

void sdl3_mixer_unlock(void)
{
	if (g_mx.stream != NULL) {
		SDL_UnlockAudioStream(g_mx.stream);
	}
}

void sdl3_mixer_set_game_paused(bool paused)
{
	if (g_mx.game_paused == paused) {
		return;
	}

	g_mx.game_paused = paused;
	if (g_mx.stream == NULL) {
		return;
	}

	SDL_LockAudioStream(g_mx.stream);
	if (paused) {
		SDL_ClearAudioStream(g_mx.stream);
	} else {
		SDL_ClearAudioStream(g_mx.stream);
		g_mx.last_pump_ticks = (Uint64)SDL_GetTicks();
	}
	SDL_UnlockAudioStream(g_mx.stream);
}

bool sdl3_mixer_is_game_paused(void)
{
	return g_mx.game_paused;
}

void sdl3_mixer_pump(unsigned int milliseconds)
{
	int min_bytes;
	int target_bytes;
	int max_bytes;
	int goal_bytes;
	int frame_bytes;
	int queued;
	Uint64 now;
	unsigned int ms;

	if (g_mx.stream == NULL || g_mx.output_rate <= 0 || g_mx.bytes_per_frame <= 0) {
		return;
	}
	if (g_mx.game_paused) {
		return;
	}

	now = (Uint64)SDL_GetTicks();
	ms = milliseconds;
	if (ms == 0) {
		if (g_mx.last_pump_ticks == 0) {
			ms = 16;
		} else {
			ms = (unsigned int)(now - g_mx.last_pump_ticks);
			if (ms == 0) {
				ms = 1;
			}
			if (ms > 50) {
				ms = 50;
			}
		}
	}
	g_mx.last_pump_ticks = now;

	min_bytes = sdl3_mixer_min_queued_bytes();
	target_bytes = sdl3_mixer_target_queued_bytes();
	max_bytes = sdl3_mixer_max_queued_bytes();
	frame_bytes = sdl3_mixer_ms_to_bytes((int)ms);

	SDL_LockAudioStream(g_mx.stream);
	queued = SDL_GetAudioStreamQueued(g_mx.stream);
	if (queued > max_bytes) {
		/*
		** Frame hitches used to leave 500 ms queued and stall pumping — audible lag
		** until the device drained. Drop the backlog and refill to the low target.
		*/
		SDL_ClearAudioStream(g_mx.stream);
		queued = 0;
	}

	goal_bytes = queued + frame_bytes;
	if (goal_bytes < min_bytes) {
		goal_bytes = min_bytes;
	}
	if (goal_bytes > target_bytes) {
		goal_bytes = target_bytes;
	}

	while (queued < goal_bytes) {
		int need_bytes;
		int chunk_frames;

		need_bytes = goal_bytes - queued;
		chunk_frames = need_bytes / g_mx.bytes_per_frame;
		if (chunk_frames <= 0) {
			chunk_frames = 1;
		}
		if (chunk_frames > SDL3_MIXER_CHUNK_FRAMES) {
			chunk_frames = SDL3_MIXER_CHUNK_FRAMES;
		}

		sdl3_mixer_put_chunk(chunk_frames);
		queued = SDL_GetAudioStreamQueued(g_mx.stream);
		if (queued >= goal_bytes) {
			break;
		}
	}

	SDL_UnlockAudioStream(g_mx.stream);
}

} /* extern "C" */
