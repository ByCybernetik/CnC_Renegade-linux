#include "texview_loader.h"
#include "texview_renderer.h"

#include "sdl3_host.h"

#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static void Print_Usage(const char *prog)
{
	fprintf(
		stderr,
		"Usage: %s [options] <texture> [texture2 ...]\n"
		"\n"
		"  View DDS/TGA/PNG textures using the same loader as the game.\n"
		"\n"
		"Options:\n"
		"  -h, --help       Show this help\n"
		"  -1               1:1 pixel scale (default: fit to window)\n"
		"  -a               Toggle alpha checkerboard (default: on)\n"
		"\n"
		"Keys:\n"
		"  Left/Right       Previous / next texture\n"
		"  F                Toggle fit / 1:1 scale\n"
		"  A                Toggle alpha background\n"
		"  R                Reload current file\n"
		"  Esc / Q          Quit\n",
		prog);
}

static void Update_Window_Title(SDL_Window *window, const TexViewTextureInfo &info, int index, int count)
{
	char title[768];
	std::snprintf(
		title,
		sizeof(title),
		"texview [%d/%d] %s — %ux%u %s%s mips=%u",
		index + 1,
		count,
		info.path,
		info.width,
		info.height,
		TexView_Format_Name(info.format),
		info.compressed ? " (BC)" : "",
		info.mip_levels);
	SDL_SetWindowTitle(window, title);
}

static bool Load_Current(
	ww3d_vulkan::VkTexture *texture,
	TexViewTextureInfo *info,
	const std::vector<std::string> &paths,
	int index)
{
	if (index < 0 || index >= (int)paths.size()) {
		return false;
	}
	return TexView_Load_Texture(paths[(size_t)index].c_str(), texture, info);
}

int main(int argc, char **argv)
{
	std::vector<std::string> file_list;
	TexViewScaleMode scale_mode = TEXVIEW_SCALE_FIT;
	bool show_alpha = true;

	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			Print_Usage(argv[0]);
			return 0;
		}
		if (strcmp(argv[i], "-1") == 0) {
			scale_mode = TEXVIEW_SCALE_ONE_TO_ONE;
			continue;
		}
		if (strcmp(argv[i], "-a") == 0) {
			show_alpha = !show_alpha;
			continue;
		}
		if (argv[i][0] == '-') {
			fprintf(stderr, "texview: unknown option: %s\n", argv[i]);
			return 1;
		}
		file_list.push_back(argv[i]);
	}

	if (file_list.empty()) {
		Print_Usage(argv[0]);
		return 1;
	}

	Platform_Init_Early();
	if (!Platform_Init_Video_Audio()) {
		fprintf(stderr, "texview: Platform_Init_Video_Audio failed\n");
		return 1;
	}

	SDL_Window *window = Platform_Get_SDL_Window();
	if (window == nullptr) {
		fprintf(stderr, "texview: no SDL window\n");
		return 1;
	}
	SDL_SetWindowTitle(window, "texview");

	int win_w = 1024;
	int win_h = 768;
	SDL_GetWindowSize(window, &win_w, &win_h);

	TexViewRenderer renderer;
	if (!renderer.Init(window, (uint32_t)win_w, (uint32_t)win_h)) {
		fprintf(stderr, "texview: renderer init failed\n");
		Platform_Shutdown();
		return 1;
	}

	renderer.Set_Scale_Mode(scale_mode);
	renderer.Set_Show_Alpha_Background(show_alpha);

	ww3d_vulkan::VkTexture texture;
	TexViewTextureInfo info = {};
	int current = 0;

	if (!Load_Current(&texture, &info, file_list, current)) {
		fprintf(stderr, "texview: could not load %s\n", file_list[0].c_str());
		renderer.Shutdown();
		Platform_Shutdown();
		return 1;
	}

	renderer.Set_Texture(&texture);
	Update_Window_Title(window, info, current, (int)file_list.size());

	bool running = true;
	while (running) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_EVENT_QUIT:
				running = false;
				break;
			case SDL_EVENT_WINDOW_RESIZED:
				SDL_GetWindowSize(window, &win_w, &win_h);
				renderer.Resize((uint32_t)win_w, (uint32_t)win_h);
				renderer.Set_Texture(&texture);
				break;
			case SDL_EVENT_KEY_DOWN:
				switch (event.key.key) {
				case SDLK_ESCAPE:
				case SDLK_Q:
					running = false;
					break;
				case SDLK_LEFT:
					if (current > 0) {
						current--;
						if (Load_Current(&texture, &info, file_list, current)) {
							renderer.Set_Texture(&texture);
							Update_Window_Title(window, info, current, (int)file_list.size());
						}
					}
					break;
				case SDLK_RIGHT:
					if (current + 1 < (int)file_list.size()) {
						current++;
						if (Load_Current(&texture, &info, file_list, current)) {
							renderer.Set_Texture(&texture);
							Update_Window_Title(window, info, current, (int)file_list.size());
						}
					}
					break;
				case SDLK_F:
					scale_mode = (scale_mode == TEXVIEW_SCALE_FIT)
						? TEXVIEW_SCALE_ONE_TO_ONE
						: TEXVIEW_SCALE_FIT;
					renderer.Set_Scale_Mode(scale_mode);
					renderer.Set_Texture(&texture);
					break;
				case SDLK_A:
					show_alpha = !show_alpha;
					renderer.Set_Show_Alpha_Background(show_alpha);
					break;
				case SDLK_R:
					if (Load_Current(&texture, &info, file_list, current)) {
						renderer.Set_Texture(&texture);
						Update_Window_Title(window, info, current, (int)file_list.size());
					}
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}
		}

		if (!renderer.Begin_Frame()) {
			continue;
		}
		renderer.End_Frame();
	}

	texture.Destroy();
	renderer.Shutdown();
	Platform_Shutdown();
	return 0;
}
