#ifndef W3DVIEWER_SCENE_H
#define W3DVIEWER_SCENE_H

#include "w3dviewer_loader.h"

#include <cstdint>
#include <string>
#include <vector>

class HAnimClass;
class HTreeClass;

struct W3DViewerSubObject {
	std::string name;
	int32_t bone_index = 0;
	W3DViewerMesh mesh;
};

struct W3DViewerAnimation {
	std::string name;
	HAnimClass *anim = nullptr;
	int32_t num_frames = 0;
	float frame_rate = 30.0f;
};

struct W3DViewerScene {
	bool is_animated = false;
	std::string hierarchy_name;
	HTreeClass *htree = nullptr;
	std::vector<W3DViewerSubObject> sub_objects;
	std::vector<W3DViewerAnimation> animations;
	W3DViewerMesh static_mesh;

	int32_t current_anim = 0;
	float anim_time = 0.0f;
	bool anim_playing = true;
};

void W3DViewer_Init_Assets();
void W3DViewer_Shutdown_Assets();

bool W3DViewer_Load_Scene(
	const char *model_path,
	const char *extra_anim_path,
	W3DViewerScene *scene);

void W3DViewer_Scene_Update(W3DViewerScene *scene, float delta_seconds);

void W3DViewer_Scene_Select_Animation(W3DViewerScene *scene, int32_t index);
int32_t W3DViewer_Scene_Find_Animation(const W3DViewerScene &scene, const char *name);

void W3DViewer_Print_Scene_Info(const char *path, const W3DViewerScene &scene);

void W3DViewer_Mat4_From_Matrix3D(float out[16], const void *matrix3d);

#endif
