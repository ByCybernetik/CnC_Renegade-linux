/*
 * lvlview — Renegade level viewer (.mix / .lsd) using WW3D2 + PhysicsScene.
 */

#include "sdl3_host.h"
#include "file_init.h"
#include "win32_minimal.h"

#include "assetdep.h"
#include "backgroundmgr.h"
#include "definitionmgr.h"
#include "assetmgr.h"
#include "camera.h"
#include "combat.h"
#include "dx8wrapper.h"
#include "scene.h"
#include "SoundEnvironment.h"
#include "ffactorylist.h"
#include "part_ldr.h"
#include "pathmgr.h"
#include "pscene.h"
#include "refcount.h"
#include "ringobj.h"
#include "savegame.h"
#include "saveload.h"
#include "sphereobj.h"
#include "textureloader.h"
#include "WeatherMgr.h"
#include "ww3d.h"
#include "wwmath.h"
#include "wwphys.h"
#include "wwsaveload.h"
#if defined(RENEGADE_VULKAN)
#include "vk_dx8_texture.h"
#endif

#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <stdlib.h>
#include <string>

#if !defined(_WIN32)
#include <strings.h>
#include <unistd.h>
#endif

static void Exit_Process(int code)
{
	/*
	 * MultiListNodeClass uses a static ObjectPool that crashes during
	 * libc exit if nodes are still tracked.  Skip C++ static teardown.
	 */
#if !defined(_WIN32)
	_exit(code);
#else
	exit(code);
#endif
}

static SimpleSceneClass *g_background_scene = nullptr;
static SoundEnvironmentClass *g_sound_environment = nullptr;

static void Init_Background_Scene()
{
	if (g_background_scene != nullptr) {
		return;
	}
	g_background_scene = NEW_REF(SimpleSceneClass, ());
	g_sound_environment = NEW_REF(SoundEnvironmentClass, ());
	BackgroundMgrClass::Init(g_background_scene, g_sound_environment, true);
	WeatherMgrClass::Init(g_sound_environment);
}

static void Shutdown_Background_Scene()
{
	WeatherMgrClass::Shutdown();
	BackgroundMgrClass::Shutdown();
	REF_PTR_RELEASE(g_background_scene);
	REF_PTR_RELEASE(g_sound_environment);
}

static void Apply_Background_From_Level(PhysicsSceneClass *scene, CameraClass *camera)
{
	unsigned hours = 0;
	unsigned minutes = 0;
	BackgroundMgrClass::Get_Time_Of_Day(hours, minutes);
	BackgroundMgrClass::Set_Time_Of_Day(hours, minutes);

	float cloud_cover = 0.0f;
	float cloud_gloom = 0.0f;
	BackgroundMgrClass::Get_Clouds(cloud_cover, cloud_gloom);
	BackgroundMgrClass::Set_Clouds(cloud_cover, cloud_gloom, 0.0f);

	if (scene != nullptr && camera != nullptr) {
		BackgroundMgrClass::Update(scene, camera);
		WeatherMgrClass::Update(scene, camera);
	}
}

struct FlyCamera {
	Vector3 target = Vector3(0.0f, 0.0f, 0.0f);
	float distance = 50.0f;
	float sph_radius = 50.0f;
	float yaw = 0.0f;
	float pitch = 0.0f;
	bool rotating = false;
	bool panning = false;
	float last_mx = 0.0f;
	float last_my = 0.0f;
};

static void Print_Usage(const char *prog)
{
	fprintf(
		stderr,
		"Usage: %s [options] <level.mix|level.lsd>\n"
		"\n"
		"  View Renegade levels with the game engine renderer.\n"
		"\n"
		"Options:\n"
		"  --data-dir DIR   Game data root (default: current directory)\n"
		"  --info           Print level bounds and exit\n"
		"  -h, --help       Show this help\n"
		"\n"
		"Mouse: left=orbit, right=pan, wheel=zoom\n"
		"Keys:  R reset, F wireframe, Esc/Q quit\n",
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

static void Split_Level_Path(
	const char *path,
	char *dir_out,
	char *root_out,
	char *ext_out)
{
	if (dir_out != nullptr) {
		dir_out[0] = '\0';
	}
	if (root_out != nullptr) {
		root_out[0] = '\0';
	}
	if (ext_out != nullptr) {
		ext_out[0] = '\0';
	}
	if (path == nullptr || path[0] == '\0') {
		return;
	}
	_splitpath(path, nullptr, dir_out, root_out, ext_out);
}

static void Update_Camera_Aspect(CameraClass *camera, int width, int height)
{
	if (camera == nullptr || height <= 0) {
		return;
	}
	camera->Set_Aspect_Ratio((float)width / (float)height);
}

static void Apply_Fly_Camera(CameraClass *camera, const FlyCamera &cam)
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

	float near_z = cam.distance * 0.01f;
	float far_z = cam.distance + cam.sph_radius * 8.0f;
	if (near_z < 0.25f) {
		near_z = 0.25f;
	}
	if (far_z < near_z + 100.0f) {
		far_z = near_z + 100.0f;
	}
	camera->Set_Clip_Planes(near_z, far_z);
}

static void Frame_Camera_To_Level(FlyCamera *cam, PhysicsSceneClass *scene)
{
	if (cam == nullptr || scene == nullptr) {
		return;
	}

	Vector3 vmin;
	Vector3 vmax;
	scene->Get_Level_Extents(vmin, vmax);
	cam->target = 0.5f * (vmin + vmax);

	const Vector3 extent = vmax - vmin;
	cam->sph_radius = extent.Length() * 0.5f;
	if (cam->sph_radius < 10.0f) {
		cam->sph_radius = 10.0f;
	}
	cam->distance = cam->sph_radius * 1.8f;
	cam->yaw = 0.0f;
	cam->pitch = 0.25f;
}

static bool Resolve_Level_Paths(
	const char *level_path,
	StringClass *lsd_path,
	StringClass *mix_path)
{
	char dir_name[_MAX_DIR] = {0};
	char root_name[_MAX_FNAME] = {0};
	char extension[_MAX_EXT] = {0};
	Split_Level_Path(level_path, dir_name, root_name, extension);

	if (root_name[0] == '\0' || extension[0] == '\0') {
		return false;
	}

	const bool is_mix = Name_Compare(extension, ".mix") == 0;
	const bool is_lsd = Name_Compare(extension, ".lsd") == 0;
	if (!is_mix && !is_lsd) {
		return false;
	}

	if (is_mix) {
		mix_path->Format("%s", level_path);
		lsd_path->Format("%s%s.lsd", dir_name, root_name);
		return true;
	}

	lsd_path->Format("%s", level_path);
	mix_path->Format("%s%s.mix", dir_name, root_name);
	return true;
}

static std::string Normalize_Path_Slashes(const char *path)
{
	std::string normalized = path != nullptr ? path : ".";
	for (size_t i = 0; i < normalized.size(); ++i) {
		if (normalized[i] == '\\') {
			normalized[i] = '/';
		}
	}
	return normalized;
}

static std::string Deduce_Data_Dir(const char *level_path, const char *explicit_dir)
{
	if (explicit_dir != nullptr && explicit_dir[0] != '\0') {
		return explicit_dir;
	}

	const std::string path = Normalize_Path_Slashes(level_path);

	const std::string marker = "/Data/";
	const size_t pos = path.find(marker);
	if (pos != std::string::npos) {
		return pos == 0 ? std::string(".") : path.substr(0, pos);
	}

	if (path.rfind("Data/", 0) == 0 || path.rfind("data/", 0) == 0) {
		return ".";
	}

	const size_t slash = path.find_last_of('/');
	if (slash != std::string::npos) {
		const std::string parent = path.substr(0, slash);
		if (parent == "Data" || parent == "data") {
			return ".";
		}
		return parent;
	}
	return ".";
}

static std::string Level_Path_Relative_To_Data_Dir(
	const char *level_path,
	const std::string &data_dir)
{
	std::string path = Normalize_Path_Slashes(level_path);
	if (!data_dir.empty() && data_dir != ".") {
		const std::string prefix = data_dir + "/";
		if (path.rfind(prefix, 0) == 0) {
			path = path.substr(prefix.size());
		}
	}
	return path;
}

static bool Load_Level_Data(const char *level_path, bool preload_assets)
{
	StringClass lsd_path;
	StringClass mix_path;
	if (!Resolve_Level_Paths(level_path, &lsd_path, &mix_path)) {
		fprintf(stderr, "lvlview: unsupported file type (use .mix or .lsd)\n");
		return false;
	}

	StringClass filename_to_load;
	StringClass level_lsd;
	SaveGameManager::Pre_Load_Game(mix_path, filename_to_load, level_lsd);

	char mix_root[_MAX_FNAME] = {0};
	char mix_ext[_MAX_EXT] = {0};
	Split_Level_Path(mix_path, nullptr, mix_root, mix_ext);
	if (mix_root[0] != '\0' && Name_Compare(mix_ext, ".mix") == 0) {
		StringClass mix_basename;
		mix_basename.Format("%s.mix", mix_root);
		if (FileFactoryListClass::Get_Instance() != nullptr) {
			FileFactoryListClass::Get_Instance()->Set_Search_Start(mix_basename);
		}
	}

	if (preload_assets) {
		AssetDependencyManager::Load_Always_Assets();
		AssetDependencyManager::Load_Level_Assets(level_lsd);
	}

	// MIX archives index files by basename (e.g. "M01.lsd"), not full paths.
	SaveGameManager::Set_Map_Filename(level_lsd);

	SaveGameManager::Load_Level();
	if (!SaveLoadSystemClass::Post_Load_Processing(nullptr)) {
		fprintf(stderr, "lvlview: Post_Load_Processing failed\n");
		return false;
	}

	PhysicsSceneClass *scene_after_load = PhysicsSceneClass::Get_Instance();
	if (scene_after_load != nullptr) {
		scene_after_load->Post_Load_Level_Static_Objects();
	}

	if (WW3D::Is_Initted()) {
		TextureLoader::Update(nullptr);
#if defined(RENEGADE_VULKAN)
		ww3d_vulkan::Warmup_All_File_Textures();
#endif
	}

	PhysicsSceneClass *scene = PhysicsSceneClass::Get_Instance();
	if (scene == nullptr) {
		fprintf(stderr, "lvlview: no physics scene after load\n");
		return false;
	}

	fprintf(stderr, "lvlview: loaded %s\n", (const char *)level_lsd);
	return true;
}

static void Shutdown_Subsystems(void)
{
	if (PhysicsSceneClass::Get_Instance() != nullptr) {
		PhysicsSceneClass::Get_Instance()->Remove_All();
		PhysicsSceneClass::Get_Instance()->Release_Ref();
	}

	PathMgrClass::Shutdown();
	DefinitionMgrClass::Free_Definitions();
	WWPhys::Shutdown();
	WWSaveLoad::Shutdown();
}

static bool Print_Level_Info(const char *level_path, const char *data_dir)
{
	char *cwd_buf = getcwd(nullptr, 0);
	const std::string saved_cwd = cwd_buf != nullptr ? cwd_buf : ".";
	free(cwd_buf);

	if (chdir(data_dir) != 0) {
		fprintf(stderr, "lvlview: chdir(%s) failed\n", data_dir);
		return false;
	}

	if (!Lvlview_Init_File_Factory(data_dir)) {
		chdir(saved_cwd.c_str());
		return false;
	}

	WWMath::Init();

	WW3DAssetManager *mgr = new WW3DAssetManager();
	mgr->Set_WW3D_Load_On_Demand(true);
	mgr->Register_Prototype_Loader(&_ParticleEmitterLoader);
	mgr->Register_Prototype_Loader(&_SphereLoader);
	mgr->Register_Prototype_Loader(&_RingLoader);

	PathMgrClass::Initialize();
	WWPhys::Init();
	WWSaveLoad::Init();

	SaveGameManager::Load_Definitions();
	CombatManager::Scene_Init();

	const bool ok = Load_Level_Data(level_path, false);
	if (ok) {
		PhysicsSceneClass *scene = PhysicsSceneClass::Get_Instance();
		int static_count = 0;
		for (RefPhysListIterator it = scene->Get_Static_Object_Iterator();
			 !it.Is_Done();
			 it.Next()) {
			++static_count;
		}

		Vector3 vmin;
		Vector3 vmax;
		scene->Get_Level_Extents(vmin, vmax);
		printf("Level: %s\n", level_path);
		printf("  static objects: %d\n", static_count);
		printf(
			"  bounds min=(%.2f, %.2f, %.2f)\n",
			vmin.X,
			vmin.Y,
			vmin.Z);
		printf(
			"  bounds max=(%.2f, %.2f, %.2f)\n",
			vmax.X,
			vmax.Y,
			vmax.Z);
		printf(
			"  size   =(%.2f, %.2f, %.2f)\n",
			vmax.X - vmin.X,
			vmax.Y - vmin.Y,
			vmax.Z - vmin.Z);
		fflush(stdout);
	}

	Shutdown_Subsystems();

	if (WW3DAssetManager::Get_Instance() != nullptr) {
		WW3DAssetManager::Get_Instance()->Free_Assets();
	}
	WW3DAssetManager::Delete_This();

	Lvlview_Shutdown_File_Factory();
	WWMath::Shutdown();
	chdir(saved_cwd.c_str());
	return ok;
}

static bool Init_Engine(HWND hwnd, int width, int height, const char *data_dir)
{
	WWMath::Init();

	if (!Lvlview_Init_File_Factory(data_dir)) {
		return false;
	}

	WW3DAssetManager *mgr = new WW3DAssetManager();
	mgr->Set_WW3D_Load_On_Demand(true);
	mgr->Register_Prototype_Loader(&_ParticleEmitterLoader);
	mgr->Register_Prototype_Loader(&_SphereLoader);
	mgr->Register_Prototype_Loader(&_RingLoader);
	mgr->Set_Activate_Fog_On_Load(true);

	PathMgrClass::Initialize();

	if (WW3D::Init(hwnd, nullptr, false) != WW3D_ERROR_OK) {
		fprintf(stderr, "lvlview: WW3D::Init failed\n");
		return false;
	}

	// Thumbnail .thu files are not built on Linux; load full DDS from MIX directly.
	WW3D::Set_Thumbnail_Enabled(false);

	if (WW3D::Set_Render_Device(-1, width, height, 32, 1, true) != WW3D_ERROR_OK) {
		if (WW3D::Set_Any_Render_Device() != WW3D_ERROR_OK) {
			fprintf(stderr, "lvlview: no render device\n");
			return false;
		}
	}

	WW3D::Enable_Static_Sort_Lists(true);

	WWPhys::Init();
	WWSaveLoad::Init();

	SaveGameManager::Load_Definitions();
	CombatManager::Scene_Init();
	Init_Background_Scene();

	return true;
}

static void Shutdown_Engine()
{
	Shutdown_Background_Scene();
	Shutdown_Subsystems();

	if (WW3DAssetManager::Get_Instance() != nullptr) {
		WW3DAssetManager::Get_Instance()->Free_Assets();
	}

	WW3D::Shutdown();
	WW3DAssetManager::Delete_This();
	Lvlview_Shutdown_File_Factory();
	WWMath::Shutdown();
}

int main(int argc, char **argv)
{
	const char *level_path = nullptr;
	const char *explicit_data_dir = nullptr;
	bool info_only = false;

	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--info") == 0) {
			info_only = true;
		} else if (strcmp(argv[i], "--data-dir") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "lvlview: --data-dir requires a path\n");
				return 1;
			}
			explicit_data_dir = argv[++i];
		} else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			Print_Usage(argv[0]);
			return 0;
		} else if (argv[i][0] == '-') {
			fprintf(stderr, "lvlview: unknown option %s\n", argv[i]);
			Print_Usage(argv[0]);
			return 1;
		} else if (level_path == nullptr) {
			level_path = argv[i];
		} else {
			fprintf(stderr, "lvlview: unexpected argument %s\n", argv[i]);
			Print_Usage(argv[0]);
			return 1;
		}
	}

	if (level_path == nullptr) {
		Print_Usage(argv[0]);
		return 1;
	}

	const std::string data_dir = Deduce_Data_Dir(level_path, explicit_data_dir);
	const std::string level_path_rel =
		Level_Path_Relative_To_Data_Dir(level_path, data_dir);
	const char *data_dir_c = data_dir.c_str();
	const char *level_path_c = level_path_rel.c_str();
	fprintf(stderr, "lvlview: data dir: %s\n", data_dir_c);
	fprintf(stderr, "lvlview: level: %s\n", level_path_c);

	if (info_only) {
		Exit_Process(Print_Level_Info(level_path_c, data_dir_c) ? 0 : 1);
	}

	char *cwd_buf = getcwd(nullptr, 0);
	const std::string saved_cwd = cwd_buf != nullptr ? cwd_buf : ".";
	free(cwd_buf);
	if (chdir(data_dir_c) != 0) {
		fprintf(stderr, "lvlview: chdir(%s) failed\n", data_dir_c);
		return 1;
	}

	Platform_Init_Early();
	if (!Platform_Init_Video_Audio()) {
		fprintf(stderr, "lvlview: Platform_Init_Video_Audio failed\n");
		chdir(saved_cwd.c_str());
		return 1;
	}

	SDL_Window *window = Platform_Get_SDL_Window();
	if (window == nullptr) {
		fprintf(stderr, "lvlview: no SDL window\n");
		Platform_Shutdown();
		chdir(saved_cwd.c_str());
		return 1;
	}

	int win_w = 1280;
	int win_h = 720;
	SDL_GetWindowSize(window, &win_w, &win_h);

	if (!Init_Engine((HWND)window, win_w, win_h, data_dir_c)) {
		Platform_Shutdown();
		chdir(saved_cwd.c_str());
		Exit_Process(1);
	}

	// Load geometry from the LSD; textures stream in via TextureLoader::Update.
	if (!Load_Level_Data(level_path_c, false)) {
		Shutdown_Engine();
		Platform_Shutdown();
		chdir(saved_cwd.c_str());
		Exit_Process(1);
	}

	PhysicsSceneClass *scene = PhysicsSceneClass::Get_Instance();
	CameraClass *camera = new CameraClass();
	camera->Set_Clip_Planes(0.25f, 5000.0f);
	Update_Camera_Aspect(camera, win_w, win_h);

	FlyCamera fly;
	Frame_Camera_To_Level(&fly, scene);
	Apply_Fly_Camera(camera, fly);
	Apply_Background_From_Level(scene, camera);

	bool wireframe = false;
	std::string title =
		std::string("lvlview — ") + Basename_No_Ext(level_path_c);
	SDL_SetWindowTitle(window, title.c_str());

	bool running = true;
	while (running) {
		WW3D::Sync(SDL_GetTicks());

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
				Apply_Fly_Camera(camera, fly);
				break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
				if (event.button.button == SDL_BUTTON_LEFT) {
					fly.rotating = true;
					fly.last_mx = event.button.x;
					fly.last_my = event.button.y;
				} else if (event.button.button == SDL_BUTTON_RIGHT) {
					fly.panning = true;
					fly.last_mx = event.button.x;
					fly.last_my = event.button.y;
				}
				break;
			case SDL_EVENT_MOUSE_BUTTON_UP:
				if (event.button.button == SDL_BUTTON_LEFT) {
					fly.rotating = false;
				}
				if (event.button.button == SDL_BUTTON_RIGHT) {
					fly.panning = false;
				}
				break;
			case SDL_EVENT_MOUSE_MOTION:
				if (fly.rotating) {
					const float dx = event.motion.x - fly.last_mx;
					const float dy = event.motion.y - fly.last_my;
					fly.yaw -= dx * 0.005f;
					fly.pitch -= dy * 0.005f;
					if (fly.pitch < -1.55f) {
						fly.pitch = -1.55f;
					}
					if (fly.pitch > 1.55f) {
						fly.pitch = 1.55f;
					}
					fly.last_mx = event.motion.x;
					fly.last_my = event.motion.y;
					Apply_Fly_Camera(camera, fly);
				} else if (fly.panning) {
					const float dx = event.motion.x - fly.last_mx;
					const float dy = event.motion.y - fly.last_my;
					const float pan_speed = fly.distance * 0.001f;
					fly.target.X -= dx * pan_speed * cosf(fly.yaw);
					fly.target.Y += dx * pan_speed * sinf(fly.yaw);
					fly.target.Z += dy * pan_speed;
					fly.last_mx = event.motion.x;
					fly.last_my = event.motion.y;
					Apply_Fly_Camera(camera, fly);
				}
				break;
			case SDL_EVENT_MOUSE_WHEEL:
				fly.distance -= event.wheel.y * fly.distance * 0.1f;
				if (fly.distance < 1.0f) {
					fly.distance = 1.0f;
				}
				Apply_Fly_Camera(camera, fly);
				break;
			case SDL_EVENT_KEY_DOWN:
				switch (event.key.key) {
				case SDLK_ESCAPE:
				case SDLK_Q:
					running = false;
					break;
				case SDLK_R:
					Frame_Camera_To_Level(&fly, scene);
					Apply_Fly_Camera(camera, fly);
					break;
				case SDLK_F:
					wireframe = !wireframe;
					DX8Wrapper::Set_DX8_Render_State(
						D3DRS_FILLMODE,
						wireframe ? D3DFILL_WIREFRAME : D3DFILL_SOLID);
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}
		}

		if (g_sound_environment != nullptr) {
			g_sound_environment->Update(scene, camera);
		}
		BackgroundMgrClass::Update(scene, camera);
		WeatherMgrClass::Update(scene, camera);

		scene->Pre_Render_Processing(*camera);
		TextureLoader::Update(nullptr);

		WW3D::Begin_Render(true, true, BackgroundMgrClass::Get_Clear_Color());
		if (g_background_scene != nullptr) {
			WW3D::Render(g_background_scene, camera);
		}
		WW3D::Render(scene, camera);
		WW3D::End_Render(true);
		scene->Post_Render_Processing();
	}

	REF_PTR_RELEASE(camera);
	Shutdown_Engine();
	Platform_Shutdown();
	chdir(saved_cwd.c_str());
	Exit_Process(0);
}
