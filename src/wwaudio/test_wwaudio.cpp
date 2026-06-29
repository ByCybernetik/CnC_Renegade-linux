/*
 * Standalone test player for WWAudioClass.
 * Exercises the full wwaudio stack: init, file I/O via MIX, 2D/3D sounds,
 * background music, volume/pan control, playlist, and clean shutdown.
 *
 * Build:
 *   ninja -C build-linux-vulkan test_wwaudio
 *
 * Usage:
 *   ./test_wwaudio --data-dir game --sound interface_mainmove.wav
 *   ./test_wwaudio --data-dir game --music --sound rain_loop.wav
 *   ./test_wwaudio --data-dir game --sound explosion.wav --volume 0.5 --pan -0.8
 *   ./test_wwaudio --data-dir game --mode stress --sound weapon_fire.wav
 */

#include "always.h"
#include <windows.h>
#include <SDL3/SDL.h>
#include "wwaudio.h"
#include "audiblesound.h"
#include "sound3d.h"
#include "soundpseudo3d.h"
#include "ffactory.h"
#include "mixfile.h"
#include "wwdebug.h"
#include "ww3d.h"
#include "simplevec.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <unistd.h>
#include <dirent.h>
#include <strings.h>

/* ------------------------------------------------------------------ */
/* Minimal FileFactoryListClass (avoids linking combat_lib)            */
/* ------------------------------------------------------------------ */

class TestFileFactoryListClass : public FileFactoryClass {
public:
	TestFileFactoryListClass() : SearchStartIndex(0) {}
	~TestFileFactoryListClass() {}

	void Add(FileFactoryClass *factory) {
		FactoryList.Add(factory);
	}

	virtual FileClass *Get_File(const char *filename) {
		/* Try each registered factory in order */
		for (int i = SearchStartIndex; i < FactoryList.Count(); i++) {
			FileClass *file = FactoryList[i]->Get_File(filename);
			if (file && file->Is_Available()) {
				return file;
			}
			if (file) {
				FactoryList[i]->Return_File(file);
			}
		}
		/* Retry from start */
		for (int i = 0; i < SearchStartIndex; i++) {
			FileClass *file = FactoryList[i]->Get_File(filename);
			if (file && file->Is_Available()) {
				return file;
			}
			if (file) {
				FactoryList[i]->Return_File(file);
			}
		}
		return NULL;
	}

	virtual void Return_File(FileClass *file) {
		if (!file) return;
		/* Find which factory owns it and return to that factory */
		for (int i = 0; i < FactoryList.Count(); i++) {
			/* Try returning to each factory — first one that accepts wins */
			FactoryList[i]->Return_File(file);
			return;
		}
		delete file;
	}

private:
	SimpleDynVecClass<FileFactoryClass *> FactoryList;
	int SearchStartIndex;
};

/* ------------------------------------------------------------------ */
/* Globals                                                             */
/* ------------------------------------------------------------------ */

static volatile int g_running = 1;
static SimpleFileFactoryClass g_base_factory;
static TestFileFactoryListClass g_file_factory_list;

static const char *DATA_SUBDIRECTORY    = "Data/";
static const char *SAVE_SUBDIRECTORY    = "Data/save/";
static const char *CONFIG_SUBDIRECTORY  = "Data/config/";

/* ------------------------------------------------------------------ */
/* Signal handler                                                     */
/* ------------------------------------------------------------------ */

static void handle_sigint(int) { g_running = 0; }

/* ------------------------------------------------------------------ */
/* File factory setup (adapted from lvlview/file_init.cpp)            */
/* ------------------------------------------------------------------ */

static int name_compare_i(const char *a, const char *b)
{
	return strcasecmp(a, b);
}

static bool is_mix_archive(const char *filename)
{
	if (!filename || filename[0] == '\0') return false;
	size_t len = strlen(filename);
	if (len < 5) return false;
	const char *ext = filename + len - 4;
	return name_compare_i(ext, ".mix") == 0 ||
	       name_compare_i(ext, ".dat") == 0 ||
	       name_compare_i(ext, ".dbs") == 0;
}

static void scan_mix_in_dir(const char *dir_path)
{
	DIR *dir = opendir(dir_path);
	if (!dir) return;
	while (struct dirent *entry = readdir(dir)) {
		if (entry->d_name[0] == '.') continue;
		if (!is_mix_archive(entry->d_name)) continue;
		if (name_compare_i(entry->d_name, "always.dbs") == 0 ||
		    name_compare_i(entry->d_name, "always.dat") == 0)
			continue;
		MixFileFactoryClass *mix = new MixFileFactoryClass(entry->d_name, &g_base_factory);
		g_file_factory_list.Add(mix);
	}
	closedir(dir);
}

static void init_file_factory(const char *data_dir)
{
	if (!data_dir || data_dir[0] == '\0') data_dir = ".";

	/*
	 * Build base factory search path.
	 * The search includes data_dir itself (for always.dat/always.dbs)
	 * and data_dir/Data/ (for loose WAV files).
	 * Paths use forward slashes for Linux compatibility.
	 */
	char data_path[512], save_path[512], config_path[512];
	snprintf(data_path, sizeof(data_path), "%s/Data/", data_dir);
	snprintf(save_path, sizeof(save_path), "%s/Data/save/", data_dir);
	snprintf(config_path, sizeof(config_path), "%s/Data/config/", data_dir);

	char root_path[512];
	snprintf(root_path, sizeof(root_path), "%s/", data_dir);

	/* Semicolon-separated search path: always.dat is in data_dir root */
	StringClass search_path(false);
	search_path.Format("%s;%s;%s;%s", root_path, data_path, save_path, config_path);

	g_base_factory.Set_Sub_Directory(search_path.Peek_Buffer());

	_TheSimpleFileFactory->Set_Sub_Directory(search_path.Peek_Buffer());
	_TheSimpleFileFactory->Set_Strip_Path(true);

	/* Add base factory first (handles direct file access) */
	g_file_factory_list.Add(&g_base_factory);

	/* Scan for MIX archives */
	scan_mix_in_dir(data_dir);
	if (name_compare_i(data_dir, ".") != 0) {
		char subdir[512];
		snprintf(subdir, sizeof(subdir), "%s/Data", data_dir);
		scan_mix_in_dir(subdir);
	}

	/* Register always.dbs / always.dat (the main archive) */
	g_file_factory_list.Add(
		new MixFileFactoryClass("always.dbs", &g_base_factory));
	g_file_factory_list.Add(
		new MixFileFactoryClass("always.dat", &g_base_factory));

	_TheFileFactory = &g_file_factory_list;
}

static void shutdown_file_factory(void)
{
	_TheFileFactory = NULL;
	_TheSimpleFileFactory->Set_Sub_Directory("");
}

/* ------------------------------------------------------------------ */
/* Print usage                                                         */
/* ------------------------------------------------------------------ */

static void print_usage(const char *prog)
{
	printf(
		"Usage: %s [options]\n"
		"\n"
		"Options:\n"
		"  --data-dir <dir>    Game data directory (default: .)\n"
		"  --sound <file>      Sound file to play (WAV/MP3/OGG)\n"
		"  --music             Play as background music (looping)\n"
		"  --volume <0..1>     Volume (default: 1.0)\n"
		"  --pan <-1..1>       Pan: -1 left, 0 center, 1 right (default: 0)\n"
		"  --mode <mode>       play|stress (default: play)\n"
		"  --loop <N>          Loop count: 0=infinite, 1=once (default: 0 if --music, else 1)\n"
		"\n"
		"Examples:\n"
		"  %s --data-dir game --sound interface_mainmove.wav\n"
		"  %s --data-dir game --music --sound rain_loop.wav\n"
		"  %s --data-dir game --mode stress --sound explosion.wav\n",
		prog, prog, prog, prog);
}

/* ------------------------------------------------------------------ */
/* Parse arguments                                                     */
/* ------------------------------------------------------------------ */

struct TestArgs {
	const char *data_dir;
	const char *sound_file;
	bool        music_mode;
	float       volume;
	float       pan;
	int         loop_count;
	const char *mode;
};

static bool parse_args(int argc, char **argv, TestArgs &args)
{
	args.data_dir   = ".";
	args.sound_file = NULL;
	args.music_mode = false;
	args.volume     = 1.0f;
	args.pan        = 0.0f;
	args.loop_count = -1;
	args.mode       = "play";

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--data-dir") == 0 && i + 1 < argc) {
			args.data_dir = argv[++i];
		} else if (strcmp(argv[i], "--sound") == 0 && i + 1 < argc) {
			args.sound_file = argv[++i];
		} else if (strcmp(argv[i], "--music") == 0) {
			args.music_mode = true;
		} else if (strcmp(argv[i], "--volume") == 0 && i + 1 < argc) {
			args.volume = (float)atof(argv[++i]);
		} else if (strcmp(argv[i], "--pan") == 0 && i + 1 < argc) {
			args.pan = (float)atof(argv[++i]);
		} else if (strcmp(argv[i], "--loop") == 0 && i + 1 < argc) {
			args.loop_count = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
			args.mode = argv[++i];
		} else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
			return false;
		}
	}

	if (!args.sound_file) return false;
	if (args.loop_count < 0) args.loop_count = args.music_mode ? 0 : 1;
	return true;
}

/* ------------------------------------------------------------------ */
/* Mode: play — play a single sound and wait                          */
/* ------------------------------------------------------------------ */

static void mode_play(WWAudioClass *audio, const TestArgs &args)
{
	printf("[play] Loading: %s\n", args.sound_file);

	AudibleSoundClass *sound = audio->Create_Sound_Effect(args.sound_file);
	if (!sound) {
		printf("[play] FAILED to create sound: %s\n", args.sound_file);
		return;
	}

	sound->Set_Volume(args.volume);
	sound->Set_Pan(args.pan);
	if (args.loop_count == 0) {
		sound->Set_Loop_Count(0); /* infinite */
	} else if (args.loop_count > 1) {
		sound->Set_Loop_Count(args.loop_count);
	}
	sound->Play();

	printf("[play] Playing: vol=%.2f pan=%.2f loop=%d\n",
	       args.volume, args.pan, args.loop_count);
	printf("[play] Press Ctrl+C to stop.\n");

	Uint64 last_update = SDL_GetPerformanceCounter();
	while (g_running) {
		Uint64 now = SDL_GetPerformanceCounter();
		Uint64 delta_ms = (now - last_update) * 1000 / SDL_GetPerformanceFrequency();
		last_update = now;

		audio->On_Frame_Update((unsigned int)delta_ms);

		/* Print status every 500ms */
		static Uint64 print_counter = 0;
		print_counter += delta_ms;
		if (print_counter >= 500) {
			print_counter = 0;
			printf("\r[play] 2D: %d  3D: %d   ",
			       audio->Get_2D_Sample_Count(),
			       audio->Get_3D_Sample_Count());
			fflush(stdout);
		}

		usleep(10000); /* 10ms tick */
	}

	printf("\n[play] Stopping...\n");
	sound->Stop();
	REF_PTR_RELEASE(sound);
}

/* ------------------------------------------------------------------ */
/* Mode: stress — rapid alloc/release cycle                           */
/* ------------------------------------------------------------------ */

static void mode_stress(WWAudioClass *audio, const TestArgs &args)
{
	printf("[stress] Rapid alloc/release test with: %s\n", args.sound_file);
	printf("[stress] Press Ctrl+C to stop.\n");

	int count = 0;
	int errors = 0;

	while (g_running) {
		AudibleSoundClass *sound = audio->Create_Sound_Effect(args.sound_file);
		if (sound) {
			sound->Set_Volume(args.volume);
			sound->Play();
			usleep(50000); /* 50ms */
			sound->Stop();
			REF_PTR_RELEASE(sound);
			count++;
		} else {
			errors++;
		}

		audio->On_Frame_Update(50);

		if (count % 50 == 0 && count > 0) {
			printf("\r[stress] cycles: %d  errors: %d  2D: %d  3D: %d   ",
			       count, errors,
			       audio->Get_2D_Sample_Count(),
			       audio->Get_3D_Sample_Count());
			fflush(stdout);
		}
	}

	printf("\n[stress] Done. cycles=%d errors=%d\n", count, errors);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
	TestArgs args;
	if (!parse_args(argc, argv, args)) {
		print_usage(argv[0]);
		return 1;
	}

	signal(SIGINT, handle_sigint);

	printf("=== WWAudioClass Standalone Test Player ===\n");
	printf("Data dir:  %s\n", args.data_dir);
	printf("Sound:     %s\n", args.sound_file);
	printf("Mode:      %s\n", args.mode);
	printf("Volume:    %.2f\n", args.volume);
	printf("Pan:       %.2f\n", args.pan);
	printf("Loop:      %d\n", args.loop_count);
	printf("Music:     %s\n", args.music_mode ? "yes" : "no");
	printf("\n");

	/* 1. Init file system */
	printf("[init] Setting up file factories...\n");
	init_file_factory(args.data_dir);

	/* 2. Init SDL3 (needed by sdl3_audio.cpp backend) */
	if (!SDL_Init(SDL_INIT_AUDIO)) {
		printf("[init] SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}

	/* 3. Create WWAudioClass */
	printf("[init] Creating WWAudioClass...\n");
	WWAudioClass *audio = new WWAudioClass(false);
	if (!audio) {
		printf("[init] FAILED to create WWAudioClass\n");
		SDL_Quit();
		return 1;
	}

	/* 4. Initialize: open 2D device, set up file callbacks */
	printf("[init] Initializing audio (stereo, 16-bit, 44100 Hz)...\n");
	audio->Initialize(true, 16, 44100);
	audio->Set_File_Factory(&g_file_factory_list);

	printf("[init] Audio ready. 2D sounds: %d, 3D sounds: %d\n",
	       audio->Get_2D_Sample_Count(), audio->Get_3D_Sample_Count());

	/* 5. If music mode, set background music */
	if (args.music_mode) {
		printf("[music] Setting background music: %s\n", args.sound_file);
		audio->Set_Background_Music(args.sound_file);
	}

	/* 6. Run mode */
	if (strcmp(args.mode, "stress") == 0) {
		mode_stress(audio, args);
	} else {
		mode_play(audio, args);
	}

	/* 7. Shutdown */
	printf("[shutdown] Cleaning up...\n");
	if (args.music_mode) {
		audio->Set_Background_Music(NULL);
	}
	audio->Flush_Cache();

	delete audio;
	audio = NULL;

	shutdown_file_factory();
	SDL_Quit();

	printf("[shutdown] Done.\n");
	return 0;
}
