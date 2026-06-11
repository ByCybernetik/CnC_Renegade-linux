/*
 * w3dview — W3D model viewer using the Renegade WW3D2 render stack (DX8 bridge + Vulkan).
 */

#include "sdl3_host.h"

#include "assetmgr.h"
#include "camera.h"
#include "dx8wrapper.h"
#include "ffactory.h"
#include "hanim.h"
#include "light.h"
#include "part_ldr.h"
#include "refcount.h"
#include "rendobj.h"
#include "ringobj.h"
#include "scene.h"
#include "sphereobj.h"
#include "ww3d.h"
#include "wwmath.h"

#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <stdlib.h>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <strings.h>
#include <unistd.h>
#endif

struct OrbitCamera {
	Vector3 target = Vector3(0.0f, 0.0f, 0.0f);
	float distance = 5.0f;
	float sph_radius = 5.0f;
	float yaw = 0.0f;
	float pitch = 0.5f;
	bool rotating = false;
	bool panning = false;
	float last_mx = 0.0f;
	float last_my = 0.0f;
};

static void Print_Usage(const char *prog)
{
	fprintf(
		stderr,
		"Usage: %s [options] <model.w3d>\n"
		"\n"
		"  View W3D models with the game engine renderer (WW3D2 + Vulkan).\n"
		"\n"
		"Options:\n"
		"  --info           Print asset stats and exit (no window)\n"
		"  --anim-file F    Extra .w3d with animations\n"
		"  --anim NAME      Initial animation (default: none)\n"
		"  --lod N          Force LOD level (default: highest)\n"
		"  -h, --help       Show this help\n"
		"\n"
		"Mouse: left=orbit, right=pan, wheel=zoom\n"
		"Keys:  R reset, Space pause anim, N/P next/prev anim, Esc/Q quit\n",
		prog);
}

static int Name_Compare(const char *a, const char *b)
{
#if defined(_WIN32)
	return _stricmp(a, b);
#else
	return strcasecmp(a, b);
#endif
}

static std::string Basename_No_Ext(const char *path)
{
	const char *slash = strrchr(path, '/');
	if (slash == nullptr) {
		slash = strrchr(path, '\\');
	}
	const char *base = (slash != nullptr) ? slash + 1 : path;
	std::string name = base;
	const size_t dot = name.rfind('.');
	if (dot != std::string::npos) {
		name.resize(dot);
	}
	return name;
}

static std::string Dirname_Copy(const char *path)
{
	std::string dir = path;
	const size_t slash = dir.find_last_of("/\\");
	if (slash == std::string::npos) {
		return ".";
	}
	dir.resize(slash);
	if (dir.empty()) {
		return "/";
	}
	return dir;
}

static bool Try_Load_W3d(WW3DAssetManager *mgr, const char *filename)
{
	if (mgr == nullptr || filename == nullptr || filename[0] == '\0') {
		return false;
	}
	if (mgr->Load_3D_Assets(filename)) {
		return true;
	}

	char lower[512];
	bool differs = false;
	strncpy(lower, filename, sizeof(lower) - 1);
	lower[sizeof(lower) - 1] = '\0';
	for (size_t i = 0; lower[i] != '\0'; ++i) {
		const char c = (char)tolower((unsigned char)lower[i]);
		if (c != lower[i]) {
			differs = true;
		}
		lower[i] = c;
	}
	return differs && mgr->Load_3D_Assets(lower);
}

static void Collect_Animation_Names(std::vector<std::string> *names)
{
	names->clear();
	WW3DAssetManager *mgr = WW3DAssetManager::Get_Instance();
	if (mgr == nullptr) {
		return;
	}
	AssetIterator *it = mgr->Create_HAnim_Iterator();
	if (it == nullptr) {
		return;
	}
	for (it->First(); !it->Is_Done(); it->Next()) {
		const char *name = it->Current_Item_Name();
		if (name != nullptr && name[0] != '\0') {
			names->push_back(name);
		}
	}
	delete it;
}

static void Print_Model_Info(const char *path, RenderObjClass *model)
{
	printf("Model file: %s\n", path);
	if (model == nullptr) {
		printf("  (failed to instantiate)\n");
		return;
	}

	const int lod_count = model->Get_LOD_Count();
	printf("  class    : %d\n", (int)model->Class_ID());
	printf("  LODs     : %d\n", lod_count);

	model->Update_Obj_Space_Bounding_Volumes();
	SphereClass sphere = model->Get_Bounding_Sphere();
	printf(
		"  bounds   : center=(%.3f, %.3f, %.3f) radius=%.3f\n",
		sphere.Center.X,
		sphere.Center.Y,
		sphere.Center.Z,
		sphere.Radius);

	std::vector<std::string> anims;
	Collect_Animation_Names(&anims);
	printf("  animations: %zu\n", anims.size());
	for (size_t i = 0; i < anims.size(); ++i) {
		HAnimClass *anim = WW3DAssetManager::Get_Instance()->Peek_HAnim(anims[i].c_str());
		int frames = anim != nullptr ? anim->Get_Num_Frames() : 0;
		float fps = anim != nullptr ? anim->Get_Frame_Rate() : 0.0f;
		printf("    [%zu] %s (%d frames, %.1f fps)\n", i, anims[i].c_str(), frames, fps);
	}
}

static void Reset_Orbit_Camera(OrbitCamera *cam, RenderObjClass *model)
{
	if (cam == nullptr || model == nullptr) {
		return;
	}
	model->Update_Obj_Space_Bounding_Volumes();
	SphereClass sphere = model->Get_Bounding_Sphere();
	cam->target = sphere.Center;
	cam->sph_radius = sphere.Radius > 0.01f ? sphere.Radius : 5.0f;
	cam->distance = cam->sph_radius * 3.0f;
	if (cam->distance < 0.5f) {
		cam->distance = 5.0f;
	}
	cam->yaw = 0.0f;
	cam->pitch = 0.0f;
}

static void Update_Camera_Aspect(CameraClass *camera, int width, int height)
{
	if (camera == nullptr || height <= 0) {
		return;
	}
	camera->Set_Aspect_Ratio((float)width / (float)height);
}

static void Apply_Orbit_To_Camera(CameraClass *camera, const OrbitCamera &cam)
{
	if (camera == nullptr) {
		return;
	}

	Vector3 eye;
	eye.X = cam.target.X + cam.distance * cosf(cam.pitch) * cosf(cam.yaw);
	eye.Y = cam.target.Y + cam.distance * cosf(cam.pitch) * sinf(cam.yaw);
	eye.Z = cam.target.Z + cam.distance * sinf(cam.pitch);

	Matrix3D tm;
	tm.Look_At(eye, cam.target, 0);
	camera->Set_Transform(tm);

	float near_z = (cam.distance - cam.sph_radius) * 0.25f;
	float far_z = cam.distance + cam.sph_radius * 4.0f;
	if (near_z < 0.1f) {
		near_z = 0.1f;
	}
	if (far_z < near_z + cam.sph_radius * 2.0f) {
		far_z = near_z + cam.sph_radius * 2.0f;
	}
	camera->Set_Clip_Planes(near_z, far_z);
}

static bool Set_Model_Animation(RenderObjClass *model, const char *anim_name)
{
	if (model == nullptr || anim_name == nullptr || anim_name[0] == '\0') {
		return false;
	}
	HAnimClass *anim = WW3DAssetManager::Get_Instance()->Get_HAnim(anim_name);
	if (anim == nullptr) {
		return false;
	}
	model->Set_Animation(anim, 0.0f, RenderObjClass::ANIM_MODE_LOOP);
	REF_PTR_RELEASE(anim);
	return true;
}

static int Find_Animation_Index(const std::vector<std::string> &names, const char *anim_name)
{
	if (anim_name == nullptr) {
		return -1;
	}
	for (size_t i = 0; i < names.size(); ++i) {
		if (Name_Compare(names[i].c_str(), anim_name) == 0) {
			return (int)i;
		}
	}
	return -1;
}

static RenderObjClass *Load_Model_Render_Obj(
	const char *model_path,
	const char *extra_anim_path)
{
	WW3DAssetManager *mgr = WW3DAssetManager::Get_Instance();
	if (mgr == nullptr) {
		return nullptr;
	}

	const std::string model_dir = Dirname_Copy(model_path);
	char *cwd_buf = getcwd(nullptr, 0);
	const std::string saved_cwd = cwd_buf != nullptr ? cwd_buf : ".";
	free(cwd_buf);
	if (chdir(model_dir.c_str()) != 0) {
		fprintf(stderr, "w3dview: warning: chdir(%s) failed\n", model_dir.c_str());
	}

	const char *file_component = strrchr(model_path, '/');
	if (file_component == nullptr) {
		file_component = strrchr(model_path, '\\');
	}
	file_component = (file_component != nullptr) ? file_component + 1 : model_path;

	if (!Try_Load_W3d(mgr, file_component)) {
		fprintf(stderr, "w3dview: Load_3D_Assets failed for %s\n", file_component);
		chdir(saved_cwd.c_str());
		return nullptr;
	}

	if (extra_anim_path != nullptr && extra_anim_path[0] != '\0') {
		const char *anim_file = strrchr(extra_anim_path, '/');
		if (anim_file == nullptr) {
			anim_file = strrchr(extra_anim_path, '\\');
		}
		anim_file = (anim_file != nullptr) ? anim_file + 1 : extra_anim_path;
		if (!Try_Load_W3d(mgr, anim_file)) {
			fprintf(stderr, "w3dview: failed to load anim file %s\n", anim_file);
		}
	}

	chdir(saved_cwd.c_str());

	const std::string obj_name = Basename_No_Ext(model_path);
	RenderObjClass *model = mgr->Create_Render_Obj(obj_name.c_str());
	if (model == nullptr) {
		fprintf(stderr, "w3dview: Create_Render_Obj(%s) failed\n", obj_name.c_str());
	}
	return model;
}

static bool Init_Engine(HWND hwnd, int width, int height)
{
	WWMath::Init();

	WW3DAssetManager *mgr = new WW3DAssetManager();
	mgr->Set_WW3D_Load_On_Demand(true);
	mgr->Register_Prototype_Loader(&_ParticleEmitterLoader);
	mgr->Register_Prototype_Loader(&_SphereLoader);
	mgr->Register_Prototype_Loader(&_RingLoader);

	if (WW3D::Init(hwnd, nullptr, false) != WW3D_ERROR_OK) {
		fprintf(stderr, "w3dview: WW3D::Init failed\n");
		return false;
	}

	if (WW3D::Set_Render_Device(-1, width, height, 32, 1, true) != WW3D_ERROR_OK) {
		if (WW3D::Set_Any_Render_Device() != WW3D_ERROR_OK) {
			fprintf(stderr, "w3dview: no render device\n");
			return false;
		}
	}

	WW3D::Enable_Static_Sort_Lists(true);
	return true;
}

static void Shutdown_Engine()
{
	if (WW3DAssetManager::Get_Instance() != nullptr) {
		WW3DAssetManager::Get_Instance()->Free_Assets();
	}
	WW3D::Shutdown();
	WW3DAssetManager::Delete_This();
	WWMath::Shutdown();
}

int main(int argc, char **argv)
{
	const char *model_path = nullptr;
	const char *anim_file = nullptr;
	const char *anim_name = nullptr;
	bool info_only = false;
	int forced_lod = -1;

	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--info") == 0) {
			info_only = true;
		} else if (strcmp(argv[i], "--anim-file") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "w3dview: --anim-file requires a path\n");
				return 1;
			}
			anim_file = argv[++i];
		} else if (strcmp(argv[i], "--anim") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "w3dview: --anim requires a name\n");
				return 1;
			}
			anim_name = argv[++i];
		} else if (strcmp(argv[i], "--lod") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "w3dview: --lod requires a number\n");
				return 1;
			}
			forced_lod = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			Print_Usage(argv[0]);
			return 0;
		} else if (argv[i][0] == '-') {
			fprintf(stderr, "w3dview: unknown option %s\n", argv[i]);
			Print_Usage(argv[0]);
			return 1;
		} else if (model_path == nullptr) {
			model_path = argv[i];
		} else {
			fprintf(stderr, "w3dview: unexpected argument %s\n", argv[i]);
			Print_Usage(argv[0]);
			return 1;
		}
	}

	if (model_path == nullptr) {
		Print_Usage(argv[0]);
		return 1;
	}

	if (info_only) {
		WW3DAssetManager *mgr = new WW3DAssetManager();
		mgr->Register_Prototype_Loader(&_ParticleEmitterLoader);
		mgr->Register_Prototype_Loader(&_SphereLoader);
		mgr->Register_Prototype_Loader(&_RingLoader);
		RenderObjClass *model = Load_Model_Render_Obj(model_path, anim_file);
		Print_Model_Info(model_path, model);
		if (model != nullptr) {
			model->Release_Ref();
		}
		WW3DAssetManager::Delete_This();
		return model != nullptr ? 0 : 1;
	}

	Platform_Init_Early();
	if (!Platform_Init_Video_Audio()) {
		fprintf(stderr, "w3dview: Platform_Init_Video_Audio failed\n");
		return 1;
	}

	SDL_Window *window = Platform_Get_SDL_Window();
	if (window == nullptr) {
		fprintf(stderr, "w3dview: no SDL window\n");
		return 1;
	}

	int win_w = 1280;
	int win_h = 720;
	SDL_GetWindowSize(window, &win_w, &win_h);
	SDL_SetWindowSize(window, win_w, win_h);

	if (!Init_Engine((HWND)window, win_w, win_h)) {
		Platform_Shutdown();
		return 1;
	}

	RenderObjClass *model = Load_Model_Render_Obj(model_path, anim_file);
	if (model == nullptr) {
		Shutdown_Engine();
		Platform_Shutdown();
		return 1;
	}

	const int lod_count = model->Get_LOD_Count();
	const int lod = forced_lod >= 0 ?
		forced_lod :
		(lod_count > 0 ? lod_count - 1 : 0);
	model->Set_LOD_Level(lod);
	model->Set_Transform(Matrix3D(1));

	std::vector<std::string> anim_names;
	Collect_Animation_Names(&anim_names);
	int current_anim = -1;
	if (anim_name != nullptr) {
		current_anim = Find_Animation_Index(anim_names, anim_name);
		if (current_anim < 0) {
			fprintf(stderr, "w3dview: animation not found: %s\n", anim_name);
		} else if (!Set_Model_Animation(model, anim_names[(size_t)current_anim].c_str())) {
			current_anim = -1;
		}
	}
	bool anim_playing = current_anim >= 0;

	SimpleSceneClass *scene = new SimpleSceneClass();
	scene->Set_Ambient_Light(Vector3(0.35f, 0.35f, 0.35f));

	LightClass *light = NEW_REF(LightClass, ());
	light->Set_Intensity(1.0f);
	light->Set_Ambient(Vector3(0.0f, 0.0f, 0.0f));
	light->Set_Diffuse(Vector3(1.0f, 1.0f, 1.0f));
	scene->Add_Render_Object(light);

	CameraClass *camera = new CameraClass();
	camera->Set_Clip_Planes(0.25f, 500.0f);
	Update_Camera_Aspect(camera, win_w, win_h);

	scene->Add_Render_Object(model);

	OrbitCamera orbit;
	Reset_Orbit_Camera(&orbit, model);
	Apply_Orbit_To_Camera(camera, orbit);

	std::string title = std::string("w3dview — ") + Basename_No_Ext(model_path);
	SDL_SetWindowTitle(window, title.c_str());

	fprintf(
		stderr,
		"w3dview: %s — LOD %d/%d, %zu animation(s)\n",
		model_path,
		lod,
		lod_count,
		anim_names.size());

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
				DX8Wrapper::Notify_Window_Resized(win_w, win_h);
				Update_Camera_Aspect(camera, win_w, win_h);
				Apply_Orbit_To_Camera(camera, orbit);
				break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
				if (event.button.button == SDL_BUTTON_LEFT) {
					orbit.rotating = true;
					orbit.last_mx = event.button.x;
					orbit.last_my = event.button.y;
				} else if (event.button.button == SDL_BUTTON_RIGHT) {
					orbit.panning = true;
					orbit.last_mx = event.button.x;
					orbit.last_my = event.button.y;
				}
				break;
			case SDL_EVENT_MOUSE_BUTTON_UP:
				if (event.button.button == SDL_BUTTON_LEFT) {
					orbit.rotating = false;
				}
				if (event.button.button == SDL_BUTTON_RIGHT) {
					orbit.panning = false;
				}
				break;
			case SDL_EVENT_MOUSE_MOTION:
				if (orbit.rotating) {
					const float dx = event.motion.x - orbit.last_mx;
					const float dy = event.motion.y - orbit.last_my;
					orbit.yaw -= dx * 0.005f;
					orbit.pitch -= dy * 0.005f;
					if (orbit.pitch < -1.55f) {
						orbit.pitch = -1.55f;
					}
					if (orbit.pitch > 1.55f) {
						orbit.pitch = 1.55f;
					}
					orbit.last_mx = event.motion.x;
					orbit.last_my = event.motion.y;
					Apply_Orbit_To_Camera(camera, orbit);
				} else if (orbit.panning) {
					const float dx = event.motion.x - orbit.last_mx;
					const float dy = event.motion.y - orbit.last_my;
					const float pan_speed = orbit.distance * 0.001f;
					orbit.target.X -= dx * pan_speed * cosf(orbit.yaw);
					orbit.target.Y += dx * pan_speed * sinf(orbit.yaw);
					orbit.target.Z += dy * pan_speed;
					orbit.last_mx = event.motion.x;
					orbit.last_my = event.motion.y;
					Apply_Orbit_To_Camera(camera, orbit);
				}
				break;
			case SDL_EVENT_MOUSE_WHEEL:
				orbit.distance -= event.wheel.y * orbit.distance * 0.1f;
				if (orbit.distance < 0.05f) {
					orbit.distance = 0.05f;
				}
				Apply_Orbit_To_Camera(camera, orbit);
				break;
			case SDL_EVENT_KEY_DOWN:
				switch (event.key.key) {
				case SDLK_ESCAPE:
				case SDLK_Q:
					running = false;
					break;
				case SDLK_R:
					Reset_Orbit_Camera(&orbit, model);
					Apply_Orbit_To_Camera(camera, orbit);
					break;
				case SDLK_SPACE:
					anim_playing = !anim_playing;
					if (anim_playing && current_anim >= 0) {
						Set_Model_Animation(model, anim_names[(size_t)current_anim].c_str());
					} else {
						model->Set_Animation();
					}
					break;
				case SDLK_N:
					if (!anim_names.empty()) {
						current_anim = (current_anim + 1) % (int)anim_names.size();
						Set_Model_Animation(model, anim_names[(size_t)current_anim].c_str());
						anim_playing = true;
						fprintf(stderr, "w3dview: anim %s\n", anim_names[(size_t)current_anim].c_str());
					}
					break;
				case SDLK_P:
					if (!anim_names.empty()) {
						current_anim = (current_anim <= 0) ?
							(int)anim_names.size() - 1 :
							current_anim - 1;
						Set_Model_Animation(model, anim_names[(size_t)current_anim].c_str());
						anim_playing = true;
						fprintf(stderr, "w3dview: anim %s\n", anim_names[(size_t)current_anim].c_str());
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

		if (!anim_playing) {
			model->Set_Animation();
		}

		model->Set_Transform(Matrix3D(1));
		light->Set_Transform(camera->Get_Transform());

		WW3D::Begin_Render(true, true, Vector3(0.12f, 0.12f, 0.14f));
		WW3D::Render(scene, camera);
		WW3D::End_Render(true);
	}

	model->Remove();
	REF_PTR_RELEASE(model);
	REF_PTR_RELEASE(light);
	REF_PTR_RELEASE(camera);
	REF_PTR_RELEASE(scene);

	Shutdown_Engine();
	Platform_Shutdown();
	return 0;
}
