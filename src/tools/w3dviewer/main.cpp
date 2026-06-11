#include "w3dviewer_loader.h"
#include "w3dviewer_renderer.h"
#include "w3dviewer_scene.h"

#include "sdl3_host.h"

#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>

static void Print_Usage(const char *prog)
{
	fprintf(
		stderr,
		"Usage: %s [options] <model.w3d>\n"
		"\n"
		"  Standalone W3D mesh viewer (Vulkan).\n"
		"\n"
		"Options:\n"
		"  --info       Print mesh stats and exit (no window)\n"
		"  --texdir DIR Extra directory to search for .png/.tga/.dds textures\n"
		"  --anim-file F  Additional .w3d with animations (hierarchy in model)\n"
		"  --anim NAME    Initial animation name (default: first in file)\n"
		"  -h, --help   Show this help\n"
		"\n"
		"Mouse:\n"
		"  Left drag    Orbit\n"
		"  Right drag   Pan\n"
		"  Wheel        Zoom\n"
		"\n"
		"Keys:\n"
		"  Arrows/W/S   Orbit / zoom\n"
		"  R            Reset camera\n"
		"  Space        Pause / resume animation\n"
		"  N / P        Next / previous animation\n"
		"  Esc / Q      Quit\n",
		prog);
}

/* Column-major 4x4 math (matches GLSL mat4 layout). */
static void Mat4_Mul(float *out, const float *a, const float *b)
{
	float tmp[16];
	for (int col = 0; col < 4; ++col) {
		for (int row = 0; row < 4; ++row) {
			tmp[col * 4 + row] =
				a[0 * 4 + row] * b[col * 4 + 0] +
				a[1 * 4 + row] * b[col * 4 + 1] +
				a[2 * 4 + row] * b[col * 4 + 2] +
				a[3 * 4 + row] * b[col * 4 + 3];
		}
	}
	memcpy(out, tmp, sizeof(tmp));
}

static void Mat4_LookAt(float *m, const float *eye, const float *target, const float *up)
{
	float f[3] = {target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]};
	float flen = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
	f[0] /= flen; f[1] /= flen; f[2] /= flen;

	float s[3] = {
		f[1] * up[2] - f[2] * up[1],
		f[2] * up[0] - f[0] * up[2],
		f[0] * up[1] - f[1] * up[0]
	};
	float slen = sqrtf(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
	s[0] /= slen; s[1] /= slen; s[2] /= slen;

	float u[3] = {
		s[1] * f[2] - s[2] * f[1],
		s[2] * f[0] - s[0] * f[2],
		s[0] * f[1] - s[1] * f[0]
	};

	m[0] = s[0]; m[1] = u[0]; m[2] = -f[0]; m[3] = 0.0f;
	m[4] = s[1]; m[5] = u[1]; m[6] = -f[1]; m[7] = 0.0f;
	m[8] = s[2]; m[9] = u[2]; m[10] = -f[2]; m[11] = 0.0f;
	m[12] = -s[0] * eye[0] - s[1] * eye[1] - s[2] * eye[2];
	m[13] = -u[0] * eye[0] - u[1] * eye[1] - u[2] * eye[2];
	m[14] = f[0] * eye[0] + f[1] * eye[1] + f[2] * eye[2];
	m[15] = 1.0f;
}

static void Mat4_Perspective(float *m, float fov_y_rad, float aspect, float near_z, float far_z)
{
	float f = 1.0f / tanf(fov_y_rad * 0.5f);
	float nf = 1.0f / (near_z - far_z);
	m[0] = f / aspect; m[1] = 0.0f; m[2] = 0.0f; m[3] = 0.0f;
	m[4] = 0.0f; m[5] = f; m[6] = 0.0f; m[7] = 0.0f;
	m[8] = 0.0f; m[9] = 0.0f; m[10] = (far_z + near_z) * nf; m[11] = -1.0f;
	m[12] = 0.0f; m[13] = 0.0f; m[14] = 2.0f * far_z * near_z * nf; m[15] = 0.0f;
}

struct OrbitCamera {
	float target[3] = {0, 0, 0};
	float distance = 5.0f;
	float sph_radius = 5.0f;
	float yaw = 0.0f;
	float pitch = 0.5f;
	bool rotating = false;
	bool panning = false;
	float last_mx = 0;
	float last_my = 0;
};

static void Camera_Reset(OrbitCamera *cam, const W3DViewerMesh &mesh)
{
	cam->target[0] = mesh.sph_center[0];
	cam->target[1] = mesh.sph_center[1];
	cam->target[2] = mesh.sph_center[2];
	cam->sph_radius = mesh.sph_radius > 0.01f ? mesh.sph_radius : 5.0f;
	cam->distance = cam->sph_radius * 3.0f;
	if (cam->distance < 0.1f) {
		cam->distance = 5.0f;
	}
	cam->yaw = 0.0f;
	cam->pitch = 0.5f;
}

static void Camera_Compute_MVP(const OrbitCamera &cam, float aspect, float *mvp)
{
	/* W3D / Westwood world: X/Y ground plane, Z = altitude (see Matrix3D::Obj_Look_At). */
	float eye[3];
	eye[0] = cam.target[0] + cam.distance * cosf(cam.pitch) * cosf(cam.yaw);
	eye[1] = cam.target[1] + cam.distance * cosf(cam.pitch) * sinf(cam.yaw);
	eye[2] = cam.target[2] + cam.distance * sinf(cam.pitch);

	float up[3] = {0.0f, 0.0f, 1.0f};
	float view[16];
	Mat4_LookAt(view, eye, cam.target, up);

	/* Tighten depth range around the model; keep near safely in front of closest verts. */
	float near_z = (cam.distance - cam.sph_radius) * 0.25f;
	float far_z = cam.distance + cam.sph_radius * 4.0f;
	if (near_z < 0.1f) {
		near_z = 0.1f;
	}
	if (far_z < near_z + cam.sph_radius * 2.0f) {
		far_z = near_z + cam.sph_radius * 2.0f;
	}

	float proj[16];
	Mat4_Perspective(proj, 45.0f * 3.14159265f / 180.0f, aspect, near_z, far_z);

	Mat4_Mul(mvp, proj, view);
}

int main(int argc, char **argv)
{
	const char *model_path = nullptr;
	const char *tex_dir = nullptr;
	const char *anim_file = nullptr;
	const char *anim_name = nullptr;
	bool info_only = false;

	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--info") == 0) {
			info_only = true;
		} else if (strcmp(argv[i], "--anim-file") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "w3dviewer: --anim-file requires a path\n");
				return 1;
			}
			anim_file = argv[++i];
		} else if (strcmp(argv[i], "--anim") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "w3dviewer: --anim requires a name\n");
				return 1;
			}
			anim_name = argv[++i];
		} else if (strcmp(argv[i], "--texdir") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "w3dviewer: --texdir requires a path\n");
				return 1;
			}
			tex_dir = argv[++i];
		} else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			Print_Usage(argv[0]);
			return 0;
		} else if (argv[i][0] == '-') {
			fprintf(stderr, "w3dviewer: unknown option %s\n", argv[i]);
			Print_Usage(argv[0]);
			return 1;
		} else if (model_path == nullptr) {
			model_path = argv[i];
		} else {
			fprintf(stderr, "w3dviewer: unexpected argument %s\n", argv[i]);
			Print_Usage(argv[0]);
			return 1;
		}
	}

	if (model_path == nullptr) {
		Print_Usage(argv[0]);
		return 1;
	}

	W3DViewerScene scene;
	if (!W3DViewer_Load_Scene(model_path, anim_file, &scene)) {
		fprintf(stderr, "w3dviewer: failed to load %s\n", model_path);
		return 1;
	}

	if (anim_name != nullptr) {
		const int32_t anim_index = W3DViewer_Scene_Find_Animation(scene, anim_name);
		if (anim_index < 0) {
			fprintf(stderr, "w3dviewer: animation not found: %s\n", anim_name);
		} else {
			W3DViewer_Scene_Select_Animation(&scene, anim_index);
		}
	}

	if (info_only) {
		W3DViewer_Print_Scene_Info(model_path, scene);
		W3DViewer_Shutdown_Assets();
		return 0;
	}

	const W3DViewerMesh &mesh = scene.static_mesh;
	fprintf(
		stderr,
		"w3dviewer: loaded %s — %u verts, %u tris, %zu textures, radius=%.2f%s\n",
		model_path,
		mesh.vertex_count,
		mesh.tri_count,
		mesh.texture_names.size(),
		mesh.sph_radius,
		scene.is_animated ? " [animated]" : "");

	Platform_Init_Early();
	if (!Platform_Init_Video_Audio()) {
		fprintf(stderr, "w3dviewer: Platform_Init_Video_Audio failed\n");
		return 1;
	}

	SDL_Window *window = Platform_Get_SDL_Window();
	if (window == nullptr) {
		fprintf(stderr, "w3dviewer: no SDL window\n");
		return 1;
	}

	std::string title = std::string("w3dviewer — ") + model_path;
	SDL_SetWindowTitle(window, title.c_str());

	int win_w = 1024;
	int win_h = 768;
	SDL_GetWindowSize(window, &win_w, &win_h);

	W3DViewerRenderer renderer;
	if (!renderer.Init(window, (uint32_t)win_w, (uint32_t)win_h)) {
		fprintf(stderr, "w3dviewer: renderer init failed\n");
		Platform_Shutdown();
		return 1;
	}
	const std::string tex_dir_str = tex_dir != nullptr ? std::string(tex_dir) : std::string();
	if (scene.is_animated) {
		W3DViewer_Scene_Update(&scene, 0.0f);
		renderer.Set_Scene(&scene, tex_dir_str);
	} else {
		renderer.Set_Mesh(&mesh, tex_dir_str);
	}

	OrbitCamera cam;
	Camera_Reset(&cam, mesh);

	float mvp[16];
	Camera_Compute_MVP(cam, (float)win_w / (float)win_h, mvp);
	renderer.Set_MVP(mvp);

	uint64_t last_ticks = SDL_GetTicks();
	bool running = true;
	while (running) {
		const uint64_t now_ticks = SDL_GetTicks();
		const float dt = (float)(now_ticks - last_ticks) / 1000.0f;
		last_ticks = now_ticks;
		if (scene.is_animated) {
			W3DViewer_Scene_Update(&scene, dt);
		}
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_EVENT_QUIT:
				running = false;
				break;
			case SDL_EVENT_WINDOW_RESIZED:
				SDL_GetWindowSize(window, &win_w, &win_h);
				renderer.Resize((uint32_t)win_w, (uint32_t)win_h);
				Camera_Compute_MVP(cam, (float)win_w / (float)win_h, mvp);
				renderer.Set_MVP(mvp);
				break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
				if (event.button.button == SDL_BUTTON_LEFT) {
					cam.rotating = true;
					cam.last_mx = event.button.x;
					cam.last_my = event.button.y;
				} else if (event.button.button == SDL_BUTTON_RIGHT) {
					cam.panning = true;
					cam.last_mx = event.button.x;
					cam.last_my = event.button.y;
				}
				break;
			case SDL_EVENT_MOUSE_BUTTON_UP:
				if (event.button.button == SDL_BUTTON_LEFT) {
					cam.rotating = false;
				}
				if (event.button.button == SDL_BUTTON_RIGHT) {
					cam.panning = false;
				}
				break;
			case SDL_EVENT_MOUSE_MOTION:
				if (cam.rotating) {
					float dx = event.motion.x - cam.last_mx;
					float dy = event.motion.y - cam.last_my;
					cam.yaw -= dx * 0.005f;
					cam.pitch -= dy * 0.005f;
					if (cam.pitch < -1.55f) cam.pitch = -1.55f;
					if (cam.pitch > 1.55f) cam.pitch = 1.55f;
					cam.last_mx = event.motion.x;
					cam.last_my = event.motion.y;
					Camera_Compute_MVP(cam, (float)win_w / (float)win_h, mvp);
					renderer.Set_MVP(mvp);
				} else if (cam.panning) {
					float dx = event.motion.x - cam.last_mx;
					float dy = event.motion.y - cam.last_my;
					float pan_speed = cam.distance * 0.001f;
					cam.target[0] -= dx * pan_speed * cosf(cam.yaw);
					cam.target[1] += dx * pan_speed * sinf(cam.yaw);
					cam.target[2] += dy * pan_speed;
					cam.last_mx = event.motion.x;
					cam.last_my = event.motion.y;
					Camera_Compute_MVP(cam, (float)win_w / (float)win_h, mvp);
					renderer.Set_MVP(mvp);
				}
				break;
			case SDL_EVENT_MOUSE_WHEEL:
				cam.distance -= event.wheel.y * cam.distance * 0.1f;
				if (cam.distance < 0.01f) cam.distance = 0.01f;
				Camera_Compute_MVP(cam, (float)win_w / (float)win_h, mvp);
				renderer.Set_MVP(mvp);
				break;
			case SDL_EVENT_KEY_DOWN:
				switch (event.key.key) {
				case SDLK_ESCAPE:
				case SDLK_Q:
					running = false;
					break;
				case SDLK_R:
					Camera_Reset(&cam, mesh);
					Camera_Compute_MVP(cam, (float)win_w / (float)win_h, mvp);
					renderer.Set_MVP(mvp);
					break;
				case SDLK_SPACE:
					if (scene.is_animated) {
						scene.anim_playing = !scene.anim_playing;
					}
					break;
				case SDLK_N:
					if (scene.is_animated && !scene.animations.empty()) {
						W3DViewer_Scene_Select_Animation(
							&scene,
							scene.current_anim + 1);
					}
					break;
				case SDLK_P:
					if (scene.is_animated && !scene.animations.empty()) {
						W3DViewer_Scene_Select_Animation(
							&scene,
							scene.current_anim - 1);
					}
					break;
				case SDLK_LEFT:
					cam.yaw -= 0.05f;
					Camera_Compute_MVP(cam, (float)win_w / (float)win_h, mvp);
					renderer.Set_MVP(mvp);
					break;
				case SDLK_RIGHT:
					cam.yaw += 0.05f;
					Camera_Compute_MVP(cam, (float)win_w / (float)win_h, mvp);
					renderer.Set_MVP(mvp);
					break;
				case SDLK_UP:
					cam.pitch += 0.05f;
					if (cam.pitch > 1.55f) cam.pitch = 1.55f;
					Camera_Compute_MVP(cam, (float)win_w / (float)win_h, mvp);
					renderer.Set_MVP(mvp);
					break;
				case SDLK_DOWN:
					cam.pitch -= 0.05f;
					if (cam.pitch < -1.55f) cam.pitch = -1.55f;
					Camera_Compute_MVP(cam, (float)win_w / (float)win_h, mvp);
					renderer.Set_MVP(mvp);
					break;
				case SDLK_W:
					cam.distance *= 0.9f;
					Camera_Compute_MVP(cam, (float)win_w / (float)win_h, mvp);
					renderer.Set_MVP(mvp);
					break;
				case SDLK_S:
					cam.distance *= 1.1f;
					Camera_Compute_MVP(cam, (float)win_w / (float)win_h, mvp);
					renderer.Set_MVP(mvp);
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

	renderer.Shutdown();
	W3DViewer_Shutdown_Assets();
	Platform_Shutdown();
	return 0;
}
