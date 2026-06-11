/*
** SDL3 + audio_decode backend implementing the Miles (AIL) stub API.
*/

#include "mss_stub.h"
#include "audio_decode.h"
#include "eax_reverb.h"

#include <SDL3/SDL.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Mutex *g_lock = NULL;
static bool g_started = false;
static SDL_AudioDeviceID g_audio_dev = 0;
static SDL_AudioSpec g_device_spec;
static DIG_DRIVER_TYPE g_driver_data;
static HDIGDRIVER g_driver = &g_driver_data;
static HPROVIDER g_3d_provider = (HPROVIDER)2;
static bool g_3d_open = false;
static char g_last_error[256] = "SDL3 audio";

static HPROVIDER g_reverb_filter = (HPROVIDER)3;
static S32 g_listener_room = 0;
static int g_device_rate = 44100;

enum SdlKind
{
	SDL_KIND_2D,
	SDL_KIND_3D,
	SDL_KIND_STREAM
};

struct SdlVoice
{
	SdlKind kind;
	SDL_AudioStream *stream;
	SDL_AudioSpec src_spec;
	unsigned char *pcm;
	unsigned long pcm_bytes;
	U32 rate;
	S32 channels;
	S32 source_channels;
	S32 bits;
	volatile F32 volume;
	volatile F32 pan;
	F32 gain_l;
	F32 gain_r;
	unsigned long pcm_cursor;
	volatile S32 loop_count;
	S32 loops_remaining;
	intptr_t user_data[8];
	F32 min_dist;
	F32 max_dist;
	F32 pos_x;
	F32 pos_y;
	F32 pos_z;
	char stream_path[260];
	Uint64 playback_start_ms;
	Uint64 playback_end_ms;
	Uint64 last_levels_ms;
	bool pcm_submitted;
	bool finished_once;
	F32 effects_level;
	bool filter_reverb;
	F32 filter_reverb_level;
	F32 filter_reverb_decay;
	F32 filter_reverb_reflect;
	EaxReverb *reverb;
	SdlVoice *track_next;
};

static SdlVoice *g_voice_track_head = NULL;

static DWORD voice_playback_duration_ms(const SdlVoice *v);
static int voice_stream_is_playing(SdlVoice *v);

static Uint64 voice_now_ms(void)
{
	return (Uint64)SDL_GetTicks();
}

static void lock_init(void)
{
	if (g_lock == NULL) {
		g_lock = SDL_CreateMutex();
	}
}

static void sdl_lock(void)
{
	lock_init();
	if (g_lock != NULL) {
		SDL_LockMutex(g_lock);
	}
}

static void sdl_unlock(void)
{
	if (g_lock != NULL) {
		SDL_UnlockMutex(g_lock);
	}
}

static void sdl_set_error(const char *where, const char *detail)
{
	snprintf(g_last_error, sizeof(g_last_error) - 1, "%s: %s", where, detail);
	g_last_error[sizeof(g_last_error) - 1] = '\0';
}

static SdlVoice *voice_from_sample(HSAMPLE s)
{
	return (SdlVoice *)s;
}

static SdlVoice *voice_from_3d(H3DSAMPLE s)
{
	return (SdlVoice *)s;
}

static SdlVoice *voice_from_stream(HSTREAM s)
{
	return (SdlVoice *)s;
}

static void voice_track_add(SdlVoice *v)
{
	if (v == NULL || v->track_next != NULL) {
		return;
	}
	v->track_next = g_voice_track_head;
	g_voice_track_head = v;
}

static void voice_track_remove(SdlVoice *v)
{
	SdlVoice **link;

	if (v == NULL) {
		return;
	}

	for (link = &g_voice_track_head; *link != NULL; link = &(*link)->track_next) {
		if (*link == v) {
			*link = v->track_next;
			v->track_next = NULL;
			return;
		}
	}
}

static void voice_reverb_apply_listener_room(void)
{
	SdlVoice *v;

	for (v = g_voice_track_head; v != NULL; v = v->track_next) {
		if (v->reverb != NULL && !v->filter_reverb) {
			eax_reverb_set_preset(v->reverb, g_listener_room);
		}
	}
}

static unsigned long voice_frame_bytes(const SdlVoice *v)
{
	if (v != NULL && v->channels == 2) {
		return 4UL;
	}
	return 2UL;
}

static unsigned long voice_bytes_per_sec(const SdlVoice *v)
{
	if (v == NULL) {
		return 0;
	}
	return (unsigned long)v->rate * (unsigned long)v->channels * (unsigned long)(v->bits / 8);
}

static void voice_pcm_seek_bytes(SdlVoice *v, unsigned long byte_pos)
{
	unsigned long frame;
	unsigned long bytes_per_sec;
	Uint64 ms;
	Uint64 now;

	if (v == NULL || v->pcm == NULL || v->pcm_bytes == 0) {
		return;
	}

	frame = voice_frame_bytes(v);
	if (frame == 0) {
		return;
	}

	if (v->loop_count == 0) {
		if (v->pcm_bytes > frame) {
			byte_pos = byte_pos % v->pcm_bytes;
		}
	} else if (byte_pos + frame > v->pcm_bytes) {
		byte_pos = (v->pcm_bytes > frame) ? (v->pcm_bytes - frame) : 0;
	}
	byte_pos -= byte_pos % frame;

	v->pcm_cursor = byte_pos;
	v->finished_once = false;

	if (v->stream != NULL) {
		SDL_ClearAudioStream(v->stream);
	}

	bytes_per_sec = voice_bytes_per_sec(v);
	now = voice_now_ms();
	if (bytes_per_sec > 0) {
		ms = (Uint64)((byte_pos * 1000UL) / bytes_per_sec);
		v->playback_start_ms = (ms <= now) ? (now - ms) : now;
	} else {
		v->playback_start_ms = now;
	}
	v->playback_end_ms = v->playback_start_ms + (Uint64)voice_playback_duration_ms(v);
}

static void voice_pcm_seek_ms(SdlVoice *v, S32 ms)
{
	unsigned long bytes_per_sec;
	unsigned long byte_pos;

	if (v == NULL) {
		return;
	}
	if (ms < 0) {
		ms = 0;
	}

	bytes_per_sec = voice_bytes_per_sec(v);
	if (bytes_per_sec == 0) {
		return;
	}

	byte_pos = (unsigned long)((unsigned long long)ms * bytes_per_sec / 1000ULL);
	voice_pcm_seek_bytes(v, byte_pos);
}

static unsigned int read_le32(const unsigned char *p)
{
	return (unsigned int)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

static void fill_src_spec(SDL_AudioSpec *spec, S32 rate, S32 channels)
{
	SDL_zero(*spec);
	spec->freq = (rate > 0) ? rate : 44100;
	spec->format = SDL_AUDIO_S16LE;
	spec->channels = (Sint8)((channels > 0) ? channels : 1);
}

static S32 sdl_infer_channels(unsigned long pcm_bytes, S32 reported, S32 bits)
{
	S32 ch = (reported > 0) ? reported : 1;
	unsigned long bytes_per_sample;

	if (bits <= 0) {
		bits = 16;
	}
	bytes_per_sample = (unsigned long)(bits / 8);
	if (bytes_per_sample == 0) {
		bytes_per_sample = 2;
	}

	if (reported >= 2 && pcm_bytes >= 4 && (pcm_bytes % (bytes_per_sample * 2)) == 0) {
		return 2;
	}

	while (ch > 1 && pcm_bytes % (ch * bytes_per_sample) != 0) {
		ch--;
	}
	if (ch < 1) {
		ch = 1;
	}
	return ch;
}

static float miles_volume_to_gain(F32 volume)
{
	/*
	** WWAudio passes Miles sample volume as integer 0..127 (Sound*Handle::Set_Sample_Volume).
	** Values 1..127 must map to 1/127..1.0. The old branch treated (0,1] as normalized
	** unity gain, so volume==1 (fade-in, pseudo-3D edge) played at full blast.
	*/
	if (volume <= 0.0f) {
		return 0.0f;
	}
	if (volume <= 127.0f) {
		return volume / 127.0f;
	}
	return 1.0f;
}

static float miles_pan_to_balance(F32 pan)
{
	float balance;

	if (pan >= 0.0f && pan <= 127.0f) {
		balance = (pan - 63.0f) / 63.0f;
	} else {
		balance = pan;
	}
	if (balance < -1.0f) {
		return -1.0f;
	}
	if (balance > 1.0f) {
		return 1.0f;
	}
	return balance;
}

static void voice_reverb_release(SdlVoice *v)
{
	if (v == NULL || v->reverb == NULL) {
		return;
	}
	eax_reverb_destroy(v->reverb);
	v->reverb = NULL;
}

static void voice_reverb_sync_preset(SdlVoice *v)
{
	if (v == NULL) {
		return;
	}

	if (v->filter_reverb) {
		if (v->reverb == NULL) {
			v->reverb = eax_reverb_create(g_device_rate);
		}
		if (v->reverb != NULL) {
			eax_reverb_set_custom(v->reverb,
				v->filter_reverb_level,
				v->filter_reverb_decay,
				0.35f,
				v->filter_reverb_reflect);
		}
		return;
	}

	if (v->effects_level <= 0.0001f) {
		voice_reverb_release(v);
		return;
	}

	if (v->reverb == NULL) {
		v->reverb = eax_reverb_create(g_device_rate);
	}
	if (v->reverb != NULL) {
		eax_reverb_set_preset(v->reverb, g_listener_room);
	}
}

static float voice_reverb_wet_gain(const SdlVoice *v)
{
	const Eax1Preset *preset;

	if (v == NULL) {
		return 0.0f;
	}

	if (v->filter_reverb) {
		return v->filter_reverb_level;
	}

	if (v->effects_level <= 0.0001f) {
		return 0.0f;
	}

	/*
	** Infinite-loop level beds: no EAX wet (mono static ambients on H3DSAMPLE were too loud
	** and diffuse; gameplay one-shots still get reverb).
	*/
	if (v->loop_count == 0) {
		return 0.0f;
	}

	preset = eax_reverb_get_preset(g_listener_room);
	return v->effects_level * preset->volume;
}

static float voice_3d_gain_scale(SdlVoice *v)
{
	float dist;
	float range;

	if (v == NULL || v->kind != SDL_KIND_3D) {
		return 1.0f;
	}

	dist = sqrtf(v->pos_x * v->pos_x + v->pos_y * v->pos_y + v->pos_z * v->pos_z);
	if (v->max_dist <= v->min_dist) {
		return (dist <= v->min_dist) ? 1.0f : 0.0f;
	}
	if (dist <= v->min_dist) {
		return 1.0f;
	}
	if (dist >= v->max_dist) {
		return 0.0f;
	}
	range = v->max_dist - v->min_dist;
	return 1.0f - ((dist - v->min_dist) / range);
}

static F32 voice_effective_pan(const SdlVoice *v)
{
	F32 pan;

	if (v == NULL) {
		return 63.0f;
	}

	pan = v->pan;
	if (v->kind == SDL_KIND_3D) {
		float angle = atan2f(-v->pos_x, v->pos_z);
		float pan_norm = (-sinf(angle) * 0.5f) + 0.5f;
		pan = pan_norm * 127.0f;
	}
	return pan;
}

static void voice_compute_stereo_gains(SdlVoice *v, float *out_l, float *out_r)
{
	float vol;
	float dist;
	float balance;
	float left;
	float right;

	if (v == NULL || out_l == NULL || out_r == NULL) {
		return;
	}

	vol = miles_volume_to_gain(v->volume);
	dist = voice_3d_gain_scale(v);
	balance = miles_pan_to_balance(voice_effective_pan(v));
	left = 1.0f;
	right = 1.0f;
	if (balance < 0.0f) {
		right = 1.0f + balance;
	} else if (balance > 0.0f) {
		left = 1.0f - balance;
	}
	*out_l = vol * dist * left;
	*out_r = vol * dist * right;

	/*
	** Infinite-loop stereo beds: pan via master gain only (faudio_mss.cpp voice_pan_gain_scale).
	** Per-channel balance on native stereo PCM collapses the ambient soundscape to mono when panned.
	*/
	if (v->loop_count == 0 && v->source_channels >= 2) {
		float master = vol * dist * ((left + right) * 0.5f);
		*out_l = master;
		*out_r = master;
	}
}

static void voice_apply_levels(SdlVoice *v)
{
	if (v == NULL || v->stream == NULL) {
		return;
	}

	voice_compute_stereo_gains(v, &v->gain_l, &v->gain_r);
	SDL_SetAudioStreamGain(v->stream, 1.0f);
	v->last_levels_ms = voice_now_ms();
}

static void voice_apply_levels_throttled(SdlVoice *v, int force)
{
	Uint64 now;

	if (v == NULL || v->stream == NULL) {
		return;
	}

	now = voice_now_ms();
	if (!force && v->last_levels_ms != 0 && (now - v->last_levels_ms) < 33) {
		return;
	}
	voice_apply_levels(v);
}

static DWORD voice_playback_duration_ms(const SdlVoice *v)
{
	unsigned long bytes_per_sec;

	if (v == NULL || v->pcm_bytes == 0) {
		return 0;
	}

	bytes_per_sec = (unsigned long)v->rate * (unsigned long)v->channels * (unsigned long)(v->bits / 8);
	if (bytes_per_sec == 0) {
		return 0;
	}
	return (DWORD)((v->pcm_bytes * 1000UL) / bytes_per_sec) + RENEGADE_AUDIO_PLAYBACK_TAIL_MS;
}

static int voice_is_oneshot_sfx(const SdlVoice *v);

static int voice_stream_is_playing(SdlVoice *v)
{
	Uint64 now;

	if (v == NULL) {
		return 0;
	}

	now = voice_now_ms();

	/*
	** One-shot SFX (loop_count 1, PutAudioStreamData): SDL_GetAudioStreamQueued can remain
	** > 0 after the clip finished (logs: 2nd reload blocked with reason=playing 4 s later).
	** Use the wall-clock play window only for these voices.
	*/
	if (voice_is_oneshot_sfx(v)) {
		if (v->playback_start_ms != 0 && now >= v->playback_start_ms && now < v->playback_end_ms) {
			return 1;
		}
		if (v->playback_start_ms != 0 && now >= v->playback_end_ms && !v->finished_once) {
			if (v->stream != NULL) {
				int queued = SDL_GetAudioStreamQueued(v->stream);
				if (queued > 0) {
					unsigned long bytes_per_sec =
						(unsigned long)v->rate * (unsigned long)v->channels * (unsigned long)(v->bits / 8);
					Uint64 drain_ms = (bytes_per_sec > 0)
						? (Uint64)((unsigned long)queued * 1000UL / bytes_per_sec)
						: 0;
					if (drain_ms > (Uint64)RENEGADE_AUDIO_PLAYBACK_TAIL_MS) {
						drain_ms = (Uint64)RENEGADE_AUDIO_PLAYBACK_TAIL_MS;
					}
					if (now < v->playback_end_ms + drain_ms) {
						return 1;
					}
				}
			}
			v->finished_once = true;
		}
		return 0;
	}

	if (v->playback_start_ms != 0 && now >= v->playback_start_ms && now < v->playback_end_ms) {
		return 1;
	}

	if (v->stream != NULL && SDL_GetAudioStreamQueued(v->stream) > 0) {
		return 1;
	}

	if (v->loop_count == 0 && v->pcm_submitted && !v->finished_once) {
		return 1;
	}

	return 0;
}

static void voice_on_pcm_end(SdlVoice *v)
{
	if (v == NULL) {
		return;
	}

	if (v->loop_count == 0) {
		v->pcm_cursor = 0;
		return;
	}

	if (v->loop_count == 1) {
		v->finished_once = true;
		return;
	}

	if (v->loops_remaining > 0) {
		v->loops_remaining--;
		if (v->loops_remaining > 0) {
			v->pcm_cursor = 0;
		} else {
			v->finished_once = true;
		}
	}
}

static void SDLCALL voice_stream_get(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
	SdlVoice *v = (SdlVoice *)userdata;
	unsigned char chunk[8192];
	int want;
	int out_frames;
	int i;
	unsigned long frame_bytes;
	float gain_l;
	float gain_r;

	(void)total_amount;

	if (v == NULL || stream == NULL || v->pcm == NULL || v->pcm_bytes == 0 || additional_amount <= 0) {
		return;
	}

	if (v->finished_once) {
		return;
	}

	/*
	** Recompute L/R gains from current pan/volume each callback — do not take g_lock here.
	** Locking from the SDL audio thread deadlocks when the main thread holds g_lock and
	** waits on PutAudioStreamData / stream drain (hang after cinematics).
	*/
	voice_compute_stereo_gains(v, &gain_l, &gain_r);

	want = additional_amount;
	if (want > (int)sizeof(chunk)) {
		want = (int)sizeof(chunk);
	}

	frame_bytes = (v->channels == 2) ? 4UL : 2UL;
	out_frames = 0;

	for (i = 0; (i * (int)frame_bytes) + (int)frame_bytes <= want; i++) {
		short sample_l;
		short sample_r;
		short *out;
		unsigned long pos;

		if (v->pcm_cursor >= v->pcm_bytes) {
			voice_on_pcm_end(v);
			if (v->finished_once || v->pcm_cursor >= v->pcm_bytes) {
				break;
			}
		}

		pos = v->pcm_cursor;
		if (v->channels == 2) {
			sample_l = *(const short *)(v->pcm + pos);
			sample_r = *(const short *)(v->pcm + pos + 2);
			v->pcm_cursor += 4;
		} else {
			sample_l = *(const short *)(v->pcm + pos);
			sample_r = sample_l;
			v->pcm_cursor += 2;
		}

		{
			float dry_l = (float)sample_l * gain_l;
			float dry_r = (float)sample_r * gain_r;
			float wet_mix = voice_reverb_wet_gain(v);
			short out_l;
			short out_r;

			if (wet_mix > 0.0001f) {
				float mono;
				float rev_l;
				float rev_r;
				float rev_mono;
				float balance;
				float pan_l;
				float pan_r;

				/*
				** Reverb send from attenuated mono; add wet on top of full stereo dry
				** (parallel dry*(1-wet) collapsed the stereo image to mono reverb).
				*/
				mono = ((dry_l + dry_r) * 0.5f) / 32768.0f;
				if (wet_mix > 1.0f) {
					wet_mix = 1.0f;
				}

				voice_reverb_sync_preset(v);
				if (v->reverb != NULL) {
					eax_reverb_process(v->reverb, mono, wet_mix, &rev_l, &rev_r);
					rev_mono = rev_l;
					balance = miles_pan_to_balance(voice_effective_pan(v));
					pan_l = 1.0f;
					pan_r = 1.0f;
					if (balance < 0.0f) {
						pan_r = 1.0f + balance;
					} else if (balance > 0.0f) {
						pan_l = 1.0f - balance;
					}
					dry_l += rev_mono * pan_l * 32767.0f;
					dry_r += rev_mono * pan_r * 32767.0f;
				}
			}

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
			out_l = (short)dry_l;
			out_r = (short)dry_r;
			out = (short *)(chunk + (size_t)i * 4);
			out[0] = out_l;
			out[1] = out_r;
		}
		out_frames++;
	}

	if (out_frames > 0) {
		SDL_PutAudioStreamData(stream, chunk, out_frames * 4);
	}
}

static void voice_teardown_stream(SdlVoice *v);

static bool voice_recreate_stream(SdlVoice *v)
{
	if (v == NULL || v->pcm == NULL || v->pcm_bytes == 0 || g_audio_dev == 0) {
		return false;
	}
	if (v->stream != NULL) {
		return true;
	}

	v->stream = SDL_CreateAudioStream(&v->src_spec, &g_device_spec);
	if (v->stream == NULL) {
		sdl_set_error("SDL_CreateAudioStream", SDL_GetError());
		return false;
	}
	if (!SDL_BindAudioStream(g_audio_dev, v->stream)) {
		sdl_set_error("SDL_BindAudioStream", SDL_GetError());
		SDL_DestroyAudioStream(v->stream);
		v->stream = NULL;
		return false;
	}
	return true;
}

static bool voice_resubmit(SdlVoice *v)
{
	if (v == NULL || v->pcm == NULL || v->pcm_bytes == 0) {
		return false;
	}
	if (!voice_recreate_stream(v)) {
		return false;
	}

	SDL_ClearAudioStream(v->stream);
	v->finished_once = false;
	v->pcm_cursor = 0;
	if (v->loop_count == 0) {
		v->loops_remaining = -1;
	} else if (v->loop_count > 1) {
		v->loops_remaining = v->loop_count;
	} else {
		v->loops_remaining = 1;
	}

	/*
	** Always pull PCM through voice_stream_get so stereo pan + 3D distance
	** update dynamically (PutAudioStreamData + scalar gain was mono-only).
	*/
	SDL_SetAudioStreamGetCallback(v->stream, voice_stream_get, v);

	voice_apply_levels(v);
	v->pcm_submitted = true;
	return true;
}

static bool voice_restart_playback(SdlVoice *v)
{
	if (v == NULL || v->pcm_bytes == 0) {
		return false;
	}
	if (!voice_recreate_stream(v)) {
		return false;
	}

	if (!v->pcm_submitted) {
		if (!voice_resubmit(v)) {
			return false;
		}
	} else if (SDL_GetAudioStreamQueued(v->stream) == 0) {
		if (v->loop_count == 1 || v->kind == SDL_KIND_STREAM) {
			if (!voice_resubmit(v)) {
				return false;
			}
		}
	}

	v->playback_start_ms = voice_now_ms();
	v->playback_end_ms = v->playback_start_ms + (Uint64)voice_playback_duration_ms(v);
	v->finished_once = false;
	return true;
}

static void voice_teardown_stream(SdlVoice *v)
{
	if (v == NULL || v->stream == NULL) {
		return;
	}

	SDL_SetAudioStreamGetCallback(v->stream, NULL, NULL);
	SDL_ClearAudioStream(v->stream);
	SDL_UnbindAudioStream(v->stream);
	SDL_DestroyAudioStream(v->stream);
	v->stream = NULL;
	v->playback_start_ms = 0;
	v->playback_end_ms = 0;
}

static void voice_clear_playback(SdlVoice *v)
{
	if (v == NULL) {
		return;
	}

	voice_teardown_stream(v);
	audio_decode_free(v->pcm);
	v->pcm = NULL;
	v->pcm_bytes = 0;
	v->pcm_submitted = false;
	v->finished_once = false;
	v->pcm_cursor = 0;
	v->source_channels = 0;
	v->stream_path[0] = '\0';
	v->playback_start_ms = 0;
	v->playback_end_ms = 0;
	memset(v->user_data, 0, sizeof(v->user_data));
}

static int voice_is_oneshot_sfx(const SdlVoice *v)
{
	return (v != NULL && v->loop_count == 1 && v->pcm_bytes > 0 && v->pcm_bytes <= 262144);
}

static void voice_destroy(SdlVoice *v)
{
	if (v == NULL) {
		return;
	}
	voice_track_remove(v);
	voice_reverb_release(v);
	voice_clear_playback(v);
	delete v;
}

static int voice_upmix_mono_pcm(unsigned char **pcm, unsigned long *pcm_bytes)
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

static bool voice_upload(SdlVoice *v, const void *file_ptr, S32 file_len)
{
	AILSOUNDINFO info;
	unsigned char *pcm = NULL;
	unsigned long pcm_bytes = 0;

	if (v == NULL || !g_started) {
		return false;
	}

	sdl_lock();

	if (file_ptr == NULL || file_len <= 0) {
		sdl_unlock();
		return false;
	}

	voice_teardown_stream(v);
	audio_decode_free(v->pcm);
	v->pcm = NULL;
	v->pcm_bytes = 0;
	v->pcm_submitted = false;

	if (!audio_decode_to_pcm(file_ptr, (unsigned long)file_len, &pcm, &pcm_bytes, &info)) {
		snprintf(g_last_error, sizeof(g_last_error) - 1, "decode failed");
		sdl_unlock();
		return false;
	}

	v->pcm = pcm;
	v->pcm_bytes = pcm_bytes;
	v->rate = info.rate;
	v->bits = 16;
	v->source_channels = (info.channels >= 2) ? 2 : 1;
	/* Trim to whole stereo frames before infer (odd tail otherwise forces mono). */
	if (info.channels >= 2 && pcm_bytes >= 4) {
		pcm_bytes &= ~(unsigned long)3;
		v->pcm_bytes = pcm_bytes;
	}
	v->channels = sdl_infer_channels(pcm_bytes, info.channels, v->bits);
	if (v->channels >= 2) {
		v->channels = 2;
	}
	if (v->channels == 2 && v->pcm_bytes >= 4) {
		v->pcm_bytes &= ~(unsigned long)3;
	}
	if (v->channels == 1) {
		if (!voice_upmix_mono_pcm(&v->pcm, &v->pcm_bytes)) {
			audio_decode_free(v->pcm);
			v->pcm = NULL;
			sdl_unlock();
			return false;
		}
		v->channels = 2;
	}
	fill_src_spec(&v->src_spec, (S32)v->rate, v->channels);

	if (v->kind == SDL_KIND_STREAM && pcm_bytes > 500000) {
		v->loop_count = 0;
	}

	if (!voice_resubmit(v)) {
		sdl_unlock();
		return false;
	}


	sdl_unlock();
	return true;
}

static bool voice_load_stream_file(SdlVoice *v, const char *filename)
{
	unsigned char *file_data = NULL;
	unsigned long file_len = 0;
	int load_ok;
	int upload_ok;

	if (v == NULL || filename == NULL || filename[0] == '\0') {
		return false;
	}

	if (v->pcm != NULL && v->pcm_bytes > 0 && v->stream_path[0] != '\0' &&
		strcmp(v->stream_path, filename) == 0 &&
		v->loop_count != 1)
	{
		return true;
	}

	load_ok = audio_load_file(filename, &file_data, &file_len);
	upload_ok = 0;
	if (load_ok) {
		upload_ok = voice_upload(v, file_data, (S32)file_len) ? 1 : 0;
		audio_decode_free(file_data);
	}

	if (upload_ok) {
		strncpy(v->stream_path, filename, sizeof(v->stream_path) - 1);
		v->stream_path[sizeof(v->stream_path) - 1] = '\0';
	} else {
		v->stream_path[0] = '\0';
	}

	return upload_ok != 0;
}

static SdlVoice *voice_allocate(SdlKind kind)
{
	SdlVoice *v = new SdlVoice;
	if (v == NULL) {
		return NULL;
	}
	memset(v, 0, sizeof(*v));
	v->kind = kind;
	v->stream_path[0] = '\0';
	v->volume = 127.0f;
	v->pan = 63.0f;
	v->gain_l = 1.0f;
	v->gain_r = 1.0f;
	v->pcm_cursor = 0;
	v->min_dist = 1.0f;
	v->max_dist = 100.0f;
	v->loop_count = 1;
	v->effects_level = 0.0f;
	v->filter_reverb = false;
	v->filter_reverb_level = 0.3f;
	v->filter_reverb_decay = 0.535f;
	v->filter_reverb_reflect = 0.01f;
	v->reverb = NULL;
	v->track_next = NULL;
	voice_track_add(v);
	return v;
}

extern "C" {

void AIL_startup(void)
{
	SDL_AudioSpec want;

	sdl_lock();
	if (!g_started) {
		lock_init();
		SDL_zero(want);
		want.freq = 48000;
		want.format = SDL_AUDIO_S16LE;
		want.channels = 2;

		g_audio_dev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &want);
		if (g_audio_dev == 0) {
			sdl_set_error("SDL_OpenAudioDevice", SDL_GetError());
		} else {
			SDL_GetAudioDeviceFormat(g_audio_dev, &g_device_spec, NULL);
			g_device_rate = (g_device_spec.freq > 0) ? g_device_spec.freq : 44100;
			eax_set_listener_room(g_listener_room);
			SDL_ResumeAudioDevice(g_audio_dev);
			g_started = true;
			g_driver_data.emulated_ds = FALSE;
			strcpy(g_last_error, "OK");
		}
	}
	sdl_unlock();
}

void AIL_shutdown(void)
{
	sdl_lock();
	if (g_audio_dev != 0) {
		SDL_CloseAudioDevice(g_audio_dev);
		g_audio_dev = 0;
	}
	g_started = false;
	g_3d_open = false;
	sdl_unlock();
}

void AIL_lock(void)
{
	sdl_lock();
}

void AIL_unlock(void)
{
	sdl_unlock();
}

S32 AIL_set_preference(U32, S32 value)
{
	return value;
}

char *AIL_last_error(void)
{
	return g_last_error;
}

S32 AIL_waveOutOpen(HDIGDRIVER *driver, LPHWAVEOUT *, unsigned int, LPWAVEFORMAT, unsigned long)
{
	if (driver != NULL) {
		*driver = g_driver;
	}
	return g_started ? AIL_NO_ERROR : -1;
}

void AIL_waveOutClose(HDIGDRIVER)
{
}

HSAMPLE AIL_allocate_sample_handle(HDIGDRIVER)
{
	if (!g_started) {
		return NULL;
	}
	return (HSAMPLE)voice_allocate(SDL_KIND_2D);
}

void AIL_release_sample_handle(HSAMPLE sample)
{
	voice_destroy(voice_from_sample(sample));
}

void AIL_init_sample(HSAMPLE sample)
{
	SdlVoice *v = voice_from_sample(sample);

	if (v == NULL) {
		return;
	}

	voice_clear_playback(v);
	v->kind = SDL_KIND_2D;
	v->stream_path[0] = '\0';
	v->pcm_submitted = false;
	v->volume = 127.0f;
	v->pan = 63.0f;
	v->loop_count = 1;
	v->last_levels_ms = 0;
	memset(v->user_data, 0, sizeof(v->user_data));
}

S32 AIL_set_named_sample_file(HSAMPLE sample, char *, void const *file_ptr, S32 file_len, S32)
{
	SdlVoice *v = voice_from_sample(sample);
	if (v == NULL) {
		return 0;
	}
	v->kind = SDL_KIND_2D;
	v->stream_path[0] = '\0';
	return voice_upload(v, file_ptr, file_len) ? 1 : 0;
}

void AIL_start_sample(HSAMPLE sample)
{
	SdlVoice *v = voice_from_sample(sample);

	sdl_lock();
	if (v != NULL && v->pcm_bytes > 0) {
		int queued = 0;
		if (v->stream != NULL) {
			queued = SDL_GetAudioStreamQueued(v->stream);
		}
		if (voice_is_oneshot_sfx(v) && (v->finished_once || (v->pcm_submitted && queued == 0))) {
			v->pcm_submitted = false;
			voice_resubmit(v);
			if (v->stream != NULL) {
				queued = SDL_GetAudioStreamQueued(v->stream);
			}
		}
		voice_restart_playback(v);
		voice_apply_levels(v);
	}
	sdl_unlock();
}

void AIL_stop_sample(HSAMPLE sample)
{
	SdlVoice *v = voice_from_sample(sample);

	sdl_lock();
	if (v != NULL) {
		v->finished_once = true;
		v->playback_start_ms = 0;
		v->playback_end_ms = 0;
		if (v->stream != NULL) {
			SDL_SetAudioStreamGetCallback(v->stream, NULL, NULL);
			SDL_ClearAudioStream(v->stream);
		}
	}
	sdl_unlock();
}

void AIL_resume_sample(HSAMPLE sample)
{
	AIL_start_sample(sample);
}

void AIL_end_sample(HSAMPLE sample)
{
	SdlVoice *v = voice_from_sample(sample);

	AIL_stop_sample(sample);
	voice_clear_playback(v);
}

S32 AIL_sample_playback_busy(HSAMPLE sample)
{
	SdlVoice *v = voice_from_sample(sample);
	if (v == NULL) {
		return 0;
	}
	return voice_stream_is_playing(v) ? 1 : 0;
}

void AIL_set_sample_pan(HSAMPLE sample, F32 pan)
{
	SdlVoice *v = voice_from_sample(sample);

	sdl_lock();
	if (v != NULL) {
		v->pan = pan;
		voice_apply_levels_throttled(v, 1);
	}
	sdl_unlock();
}

F32 AIL_sample_pan(HSAMPLE sample)
{
	SdlVoice *v = voice_from_sample(sample);
	return (v != NULL) ? v->pan : 0.0f;
}

void AIL_set_sample_volume(HSAMPLE sample, F32 volume)
{
	SdlVoice *v = voice_from_sample(sample);

	sdl_lock();
	if (v != NULL) {
		v->volume = volume;
		voice_apply_levels_throttled(v, 0);
	}
	sdl_unlock();
}

F32 AIL_sample_volume(HSAMPLE sample)
{
	SdlVoice *v = voice_from_sample(sample);
	return (v != NULL) ? v->volume : 127.0f;
}

void AIL_set_sample_loop_count(HSAMPLE sample, S32 count)
{
	SdlVoice *v = voice_from_sample(sample);
	if (v != NULL) {
		if (v->loop_count == count) {
			return;
		}
		v->loop_count = count;
		if (v->stream != NULL && v->pcm != NULL && v->pcm_bytes <= 500000) {
			voice_resubmit(v);
		}
	}
}

S32 AIL_sample_loop_count(HSAMPLE sample)
{
	SdlVoice *v = voice_from_sample(sample);
	return (v != NULL) ? v->loop_count : 0;
}

void AIL_set_sample_ms_position(HSAMPLE sample, S32 ms)
{
	SdlVoice *v = voice_from_sample(sample);
	int mid_playback;

	sdl_lock();
	if (v == NULL || v->pcm == NULL || v->pcm_bytes == 0) {
		sdl_unlock();
		return;
	}

	if (ms <= 0 && v->kind != SDL_KIND_STREAM) {
		/*
		** Seek(0) for one-shot SFX: rewind only mid-playback (see faudio_mss.cpp).
		*/
		mid_playback = (v->pcm_cursor > 0) ||
			(v->playback_start_ms != 0 && !v->finished_once && voice_stream_is_playing(v));
		if (mid_playback) {
			v->pcm_submitted = false;
			voice_resubmit(v);
		}
	} else if (ms > 0) {
		voice_pcm_seek_ms(v, ms);
		if (!v->pcm_submitted && v->stream == NULL) {
			voice_resubmit(v);
		}
	}
	sdl_unlock();
}

void AIL_sample_ms_position(HSAMPLE sample, S32 *len, S32 *pos)
{
	SdlVoice *v = voice_from_sample(sample);
	unsigned long bytes_per_sec;

	if (v == NULL) {
		if (len) {
			*len = 0;
		}
		if (pos) {
			*pos = 0;
		}
		return;
	}

	bytes_per_sec = voice_bytes_per_sec(v);
	if (len) {
		S32 duration_ms = (bytes_per_sec > 0)
			? (S32)((v->pcm_bytes * 1000) / bytes_per_sec)
			: 0;
		*len = duration_ms + RENEGADE_AUDIO_PLAYBACK_TAIL_MS;
	}
	if (pos) {
		if (bytes_per_sec > 0 && v->pcm != NULL) {
			*pos = (S32)((v->pcm_cursor * 1000UL) / bytes_per_sec);
		} else if (v->playback_start_ms != 0) {
			Uint64 elapsed = voice_now_ms() - v->playback_start_ms;
			*pos = (S32)elapsed;
		} else {
			*pos = 0;
		}
	}
}

void AIL_set_sample_user_data(HSAMPLE sample, U32 index, intptr_t data)
{
	SdlVoice *v = voice_from_sample(sample);
	if (v != NULL && index < 8u) {
		v->user_data[index] = data;
	}
}

intptr_t AIL_sample_user_data(HSAMPLE sample, U32 index)
{
	SdlVoice *v = voice_from_sample(sample);
	return (v != NULL && index < 8u) ? v->user_data[index] : 0;
}

S32 AIL_sample_playback_rate(HSAMPLE sample)
{
	SdlVoice *v = voice_from_sample(sample);
	return (v != NULL) ? (S32)v->rate : 44100;
}

void AIL_set_sample_playback_rate(HSAMPLE, S32)
{
}

void AIL_set_sample_processor(HSAMPLE sample, S32 pipeline_stage, HPROVIDER provider)
{
	SdlVoice *v = voice_from_sample(sample);

	(void)pipeline_stage;
	sdl_lock();
	if (v != NULL && provider == g_reverb_filter) {
		v->filter_reverb = true;
		voice_reverb_sync_preset(v);
	}
	sdl_unlock();
}

void AIL_set_filter_sample_preference(HSAMPLE sample, char const *name, void const *val)
{
	SdlVoice *v = voice_from_sample(sample);
	const float *fval;

	if (v == NULL || name == NULL || val == NULL) {
		return;
	}

	fval = (const float *)val;
	sdl_lock();
	if (strstr(name, "Reverb level") != NULL) {
		v->filter_reverb_level = *fval;
	} else if (strstr(name, "Reverb decay time") != NULL) {
		v->filter_reverb_decay = *fval;
	} else if (strstr(name, "Reverb reflect time") != NULL) {
		v->filter_reverb_reflect = *fval;
	}
	voice_reverb_sync_preset(v);
	sdl_unlock();
}

void AIL_set_room_type(HDIGDRIVER, S32 room_type)
{
	sdl_lock();
	if (room_type < 0 || room_type >= EAX_ENVIRONMENT_COUNT) {
		room_type = 0;
	}
	g_listener_room = room_type;
	eax_set_listener_room(room_type);
	voice_reverb_apply_listener_room();
	sdl_unlock();
}

S32 AIL_room_type(HDIGDRIVER)
{
	return g_listener_room;
}

HSTREAM AIL_open_stream_by_sample(HDIGDRIVER, HSAMPLE sample, char const *filename, S32)
{
	SdlVoice *v = voice_from_sample(sample);
	int loaded = 0;

	if (v == NULL) {
		return NULL;
	}

	voice_clear_playback(v);
	v->kind = SDL_KIND_STREAM;
	v->loop_count = 1;

	if (filename != NULL && filename[0] != '\0') {
		loaded = voice_load_stream_file(v, filename) ? 1 : 0;
		if (loaded && voice_is_oneshot_sfx(v)) {
			v->kind = SDL_KIND_2D;
		}
		if (!loaded) {
			snprintf(g_last_error, sizeof(g_last_error) - 1, "stream open failed: %s", filename);
			return NULL;
		}
	}
	return (HSTREAM)v;
}

void AIL_start_stream(HSTREAM stream)
{
	AIL_start_sample((HSAMPLE)stream);
}

void AIL_pause_stream(HSTREAM stream, S32 onoff)
{
	SdlVoice *v = voice_from_stream(stream);
	if (v == NULL) {
		return;
	}
	if (onoff) {
		AIL_stop_sample((HSAMPLE)stream);
	} else {
		AIL_resume_sample((HSAMPLE)stream);
	}
}

void AIL_close_stream(HSTREAM stream)
{
	SdlVoice *v = voice_from_stream(stream);

	sdl_lock();
	if (v != NULL && v->stream != NULL) {
		SDL_SetAudioStreamGetCallback(v->stream, NULL, NULL);
		SDL_ClearAudioStream(v->stream);
	}
	sdl_unlock();
}

void AIL_set_stream_pan(HSTREAM stream, F32 pan)
{
	AIL_set_sample_pan((HSAMPLE)stream, pan);
}

F32 AIL_stream_pan(HSTREAM stream)
{
	return AIL_sample_pan((HSAMPLE)stream);
}

void AIL_set_stream_volume(HSTREAM stream, F32 volume)
{
	AIL_set_sample_volume((HSAMPLE)stream, volume);
}

F32 AIL_stream_volume(HSTREAM stream)
{
	return AIL_sample_volume((HSAMPLE)stream);
}

void AIL_set_stream_loop_block(HSTREAM, S32, S32)
{
}

void AIL_set_stream_loop_count(HSTREAM stream, S32 count)
{
	AIL_set_sample_loop_count((HSAMPLE)stream, count);
}

S32 AIL_stream_loop_count(HSTREAM stream)
{
	return AIL_sample_loop_count((HSAMPLE)stream);
}

void AIL_set_stream_ms_position(HSTREAM stream, S32 ms)
{
	AIL_set_sample_ms_position((HSAMPLE)stream, ms);
}

void AIL_stream_ms_position(HSTREAM stream, S32 *len, S32 *pos)
{
	AIL_sample_ms_position((HSAMPLE)stream, len, pos);
}

S32 AIL_stream_playback_rate(HSTREAM stream)
{
	return AIL_sample_playback_rate((HSAMPLE)stream);
}

void AIL_set_stream_playback_rate(HSTREAM stream, S32 rate)
{
	AIL_set_sample_playback_rate((HSAMPLE)stream, rate);
}

S32 AIL_enumerate_3D_providers(HPROENUM *next, HPROVIDER *dest, char **name)
{
	if (next == NULL || *next != HPROENUM_FIRST) {
		return 0;
	}
	if (dest != NULL) {
		*dest = g_3d_provider;
	}
	if (name != NULL) {
		*name = (char *)"Renegade eax 3D";
	}
	*next = (HPROENUM)0;
	return 1;
}

S32 AIL_open_3D_provider(HPROVIDER provider)
{
	if (provider == g_3d_provider && g_started) {
		g_3d_open = true;
		return M3D_NOERR;
	}
	return -1;
}

void AIL_close_3D_provider(HPROVIDER)
{
	g_3d_open = false;
}

void AIL_set_3D_speaker_type(HPROVIDER, U32)
{
}

H3DSAMPLE AIL_allocate_3D_sample_handle(HPROVIDER provider)
{
	if (!g_3d_open || provider != g_3d_provider) {
		return NULL;
	}
	return (H3DSAMPLE)voice_allocate(SDL_KIND_3D);
}

void AIL_release_3D_sample_handle(H3DSAMPLE sample)
{
	voice_destroy(voice_from_3d(sample));
}

static unsigned long infer_file_len(const void *file_ptr)
{
	const unsigned char *d = (const unsigned char *)file_ptr;

	if (d == NULL) {
		return 0;
	}
	if (memcmp(d, "RIFF", 4) == 0) {
		return read_le32(d + 4) + 8u;
	}
	return 0;
}

S32 AIL_set_3D_sample_file_len(H3DSAMPLE sample, void const *file_ptr, S32 file_len)
{
	SdlVoice *v = voice_from_3d(sample);
	if (v == NULL || file_ptr == NULL || file_len <= 0) {
		return 0;
	}
	v->kind = SDL_KIND_3D;
	return voice_upload(v, file_ptr, file_len) ? 1 : 0;
}

S32 AIL_set_3D_sample_file(H3DSAMPLE sample, void const *file_ptr)
{
	unsigned long len = infer_file_len(file_ptr);
	if (len == 0) {
		return 0;
	}
	return AIL_set_3D_sample_file_len(sample, file_ptr, (S32)len);
}

void AIL_start_3D_sample(H3DSAMPLE sample)
{
	SdlVoice *v = voice_from_3d(sample);

	sdl_lock();
	if (v != NULL && v->pcm_bytes > 0) {
		int queued = 0;
		if (v->stream != NULL) {
			queued = SDL_GetAudioStreamQueued(v->stream);
		}
		if (voice_is_oneshot_sfx(v) && (v->finished_once || (v->pcm_submitted && queued == 0))) {
			v->pcm_submitted = false;
			voice_resubmit(v);
			if (v->stream != NULL) {
				queued = SDL_GetAudioStreamQueued(v->stream);
			}
		}
		voice_restart_playback(v);
		voice_apply_levels(v);
	}
	sdl_unlock();
}

void AIL_stop_3D_sample(H3DSAMPLE sample)
{
	AIL_stop_sample((HSAMPLE)sample);
}

void AIL_resume_3D_sample(H3DSAMPLE sample)
{
	AIL_start_3D_sample(sample);
}

void AIL_end_3D_sample(H3DSAMPLE sample)
{
	AIL_stop_3D_sample(sample);
}

void AIL_set_3D_sample_volume(H3DSAMPLE sample, F32 volume)
{
	SdlVoice *v = voice_from_3d(sample);

	sdl_lock();
	if (v != NULL) {
		v->volume = volume;
		voice_apply_levels_throttled(v, 0);
	}
	sdl_unlock();
}

F32 AIL_3D_sample_volume(H3DSAMPLE sample)
{
	SdlVoice *v = voice_from_3d(sample);
	return (v != NULL) ? v->volume : 127.0f;
}

void AIL_set_3D_sample_loop_count(H3DSAMPLE sample, S32 count)
{
	SdlVoice *v = voice_from_3d(sample);
	if (v != NULL) {
		v->loop_count = count;
		if (v->stream != NULL && v->pcm != NULL && v->pcm_bytes <= 500000) {
			voice_resubmit(v);
		}
	}
}

S32 AIL_3D_sample_loop_count(H3DSAMPLE sample)
{
	SdlVoice *v = voice_from_3d(sample);
	return (v != NULL) ? v->loop_count : 0;
}

void AIL_set_3D_sample_offset(H3DSAMPLE sample, U32 bytes)
{
	SdlVoice *v = voice_from_3d(sample);

	sdl_lock();
	if (v != NULL) {
		voice_pcm_seek_bytes(v, (unsigned long)bytes);
	}
	sdl_unlock();
}

U32 AIL_3D_sample_offset(H3DSAMPLE sample)
{
	SdlVoice *v = voice_from_3d(sample);
	return (v != NULL) ? (U32)v->pcm_cursor : 0;
}

U32 AIL_3D_sample_length(H3DSAMPLE sample)
{
	SdlVoice *v = voice_from_3d(sample);
	return (v != NULL) ? v->pcm_bytes : 0;
}

void AIL_set_3D_object_user_data(H3DSAMPLE sample, U32 index, intptr_t data)
{
	SdlVoice *v = voice_from_3d(sample);
	if (v != NULL && index < 8u) {
		v->user_data[index] = data;
	}
}

intptr_t AIL_3D_object_user_data(H3DSAMPLE sample, U32 index)
{
	SdlVoice *v = voice_from_3d(sample);
	return (v != NULL && index < 8u) ? v->user_data[index] : 0;
}

S32 AIL_3D_sample_playback_rate(H3DSAMPLE sample)
{
	SdlVoice *v = voice_from_3d(sample);
	return (v != NULL) ? (S32)v->rate : 44100;
}

void AIL_set_3D_sample_playback_rate(H3DSAMPLE, S32)
{
}

void AIL_set_3D_position(H3DSAMPLE sample, F32 x, F32 y, F32 z)
{
	SdlVoice *v = voice_from_3d(sample);

	sdl_lock();
	if (v != NULL) {
		v->pos_x = x;
		v->pos_y = y;
		v->pos_z = z;
		voice_apply_levels_throttled(v, 1);
	}
	sdl_unlock();
}

void AIL_set_3D_orientation(H3DSAMPLE, F32, F32, F32, F32, F32, F32)
{
}

void AIL_set_3D_velocity_vector(H3DSAMPLE, F32, F32, F32)
{
}

void AIL_set_3D_sample_distances(H3DSAMPLE sample, F32 max_dist, F32 min_dist)
{
	SdlVoice *v = voice_from_3d(sample);

	sdl_lock();
	if (v != NULL) {
		v->min_dist = min_dist;
		v->max_dist = max_dist;
		voice_apply_levels_throttled(v, 1);
	}
	sdl_unlock();
}

void AIL_set_3D_sample_effects_level(H3DSAMPLE sample, F32 level)
{
	SdlVoice *v = voice_from_3d(sample);

	sdl_lock();
	if (v != NULL) {
		v->effects_level = level;
		voice_reverb_sync_preset(v);
	}
	sdl_unlock();
}

H3DPOBJECT AIL_3D_open_listener(HPROVIDER)
{
	return (H3DPOBJECT)1;
}

S32 AIL_enumerate_filters(HPROENUM *next, HPROVIDER *dest, char **name)
{
	if (next != NULL && *next == HPROENUM_FIRST) {
		if (dest != NULL) {
			*dest = g_reverb_filter;
		}
		if (name != NULL) {
			*name = (char *)"Renegade eax filter";
		}
		*next = (HPROENUM)0;
		return 1;
	}
	return 0;
}

void AIL_set_file_callbacks(
	AIL_FILE_OPEN_CALLBACK open_fn,
	AIL_FILE_CLOSE_CALLBACK close_fn,
	AIL_FILE_SEEK_CALLBACK seek_fn,
	AIL_FILE_READ_CALLBACK read_fn)
{
	audio_set_miles_file_callbacks(
		(audio_file_open_fn)open_fn,
		(audio_file_close_fn)close_fn,
		(audio_file_seek_fn)seek_fn,
		(audio_file_read_fn)read_fn);
}

void AIL_stop_timer(HTIMER)
{
}

void AIL_release_timer_handle(HTIMER)
{
}

S32 AIL_WAV_info(void const *data, AILSOUNDINFO *info)
{
	if (data == NULL || info == NULL) {
		return 0;
	}
	return audio_decode_probe(data, 0xffffffff, info) ? 1 : 0;
}

} /* extern "C" */
