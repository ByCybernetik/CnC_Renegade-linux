#include "w3dviewer_scene.h"

#include <algorithm>
#include <cstdio>

#include "assetmgr.h"
#include "hanim.h"
#include "htree.h"
#include "matrix3d.h"
#include "w3d_file.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <utility>
#include <vector>

#if !defined(_WIN32)
#include <strings.h>
#endif

namespace {

struct W3dChunkView {
	uint32_t type = 0;
	uint32_t body_size = 0;
	bool has_subchunks = false;
	const uint8_t *body = nullptr;
};

static bool Read_Chunk(const uint8_t *ptr, const uint8_t *end, W3dChunkView *out)
{
	if (ptr + sizeof(W3dChunkHeader) > end) {
		return false;
	}
	const W3dChunkHeader *ch = reinterpret_cast<const W3dChunkHeader *>(ptr);
	out->type = ch->ChunkType;
	out->has_subchunks = (ch->ChunkSize & 0x80000000u) != 0;
	out->body_size = ch->ChunkSize & 0x7FFFFFFFu;
	out->body = ptr + sizeof(W3dChunkHeader);
	if (out->body + out->body_size > end) {
		return false;
	}
	return true;
}

static int Name_Compare(const char *a, const char *b)
{
#if defined(_WIN32)
	return _stricmp(a, b);
#else
	return strcasecmp(a, b);
#endif
}

static bool Find_Mesh_By_Name(
	const std::vector<std::pair<std::string, W3DViewerMesh>> &meshes,
	const std::string &name,
	W3DViewerMesh *out)
{
	for (size_t i = 0; i < meshes.size(); ++i) {
		if (Name_Compare(meshes[i].first.c_str(), name.c_str()) == 0) {
			*out = meshes[i].second;
			return true;
		}
	}
	const size_t dot = name.find('.');
	if (dot != std::string::npos) {
		const std::string short_name = name.substr(dot + 1);
		for (size_t i = 0; i < meshes.size(); ++i) {
			const std::string &mesh_name = meshes[i].first;
			const size_t mesh_dot = mesh_name.find('.');
			const char *tail = (mesh_dot == std::string::npos) ?
				mesh_name.c_str() :
				mesh_name.c_str() + mesh_dot + 1;
			if (Name_Compare(tail, short_name.c_str()) == 0) {
				*out = meshes[i].second;
				return true;
			}
		}
	}
	return false;
}

struct HlodSubRef {
	std::string name;
	uint32_t bone_index = 0;
};

static void Parse_Hlod_Lod_Array(const W3dChunkView &chunk, std::vector<HlodSubRef> *out)
{
	const uint8_t *ptr = chunk.body;
	const uint8_t *end = chunk.body + chunk.body_size;
	while (ptr < end) {
		W3dChunkView sub = {};
		if (!Read_Chunk(ptr, end, &sub)) {
			break;
		}
		if (sub.type == W3D_CHUNK_HLOD_SUB_OBJECT &&
			sub.body_size >= sizeof(W3dHLodSubObjectStruct)) {
			W3dHLodSubObjectStruct info = {};
			memcpy(&info, sub.body, sizeof(info));
			HlodSubRef ref;
			ref.name = info.Name;
			ref.bone_index = info.BoneIndex;
			if (!ref.name.empty()) {
				out->push_back(ref);
			}
		} else if (sub.has_subchunks) {
			Parse_Hlod_Lod_Array(sub, out);
		}
		ptr += sizeof(W3dChunkHeader) + sub.body_size;
	}
}

static bool Parse_Hlod_Chunk(
	const W3dChunkView &chunk,
	std::string *hierarchy_name,
	std::vector<HlodSubRef> *sub_objects)
{
	if (chunk.body_size < sizeof(W3dHLodHeaderStruct)) {
		return false;
	}
	W3dHLodHeaderStruct header = {};
	memcpy(&header, chunk.body, sizeof(header));
	*hierarchy_name = header.HierarchyName;

	const uint8_t *ptr = chunk.body;
	const uint8_t *end = chunk.body + chunk.body_size;
	while (ptr < end) {
		W3dChunkView sub = {};
		if (!Read_Chunk(ptr, end, &sub)) {
			break;
		}
		if (sub.type == W3D_CHUNK_HLOD_LOD_ARRAY) {
			Parse_Hlod_Lod_Array(sub, sub_objects);
			break;
		}
		ptr += sizeof(W3dChunkHeader) + sub.body_size;
	}
	return !sub_objects->empty();
}

static void Find_Hlod_In_Range(
	const uint8_t *data,
	uint32_t size,
	int depth,
	std::string *hierarchy_name,
	std::vector<HlodSubRef> *sub_objects)
{
	const uint8_t *ptr = data;
	const uint8_t *end = data + size;
	while (ptr < end) {
		W3dChunkView chunk = {};
		if (!Read_Chunk(ptr, end, &chunk)) {
			break;
		}
		if (chunk.type == W3D_CHUNK_HLOD) {
			if (Parse_Hlod_Chunk(chunk, hierarchy_name, sub_objects)) {
				return;
			}
		} else if (chunk.has_subchunks && depth < 8) {
			Find_Hlod_In_Range(chunk.body, chunk.body_size, depth + 1, hierarchy_name, sub_objects);
			if (!sub_objects->empty()) {
				return;
			}
		}
		ptr += sizeof(W3dChunkHeader) + chunk.body_size;
	}
}

static bool Read_File_Bytes(const char *path, std::vector<uint8_t> *out)
{
	FILE *f = fopen(path, "rb");
	if (!f) {
		return false;
	}
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (size <= 0) {
		fclose(f);
		return false;
	}
	out->resize((size_t)size);
	if (fread(out->data(), 1, (size_t)size, f) != (size_t)size) {
		fclose(f);
		return false;
	}
	fclose(f);
	return true;
}

static void Collect_Animations(W3DViewerScene *scene)
{
	scene->animations.clear();
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
		if (name == nullptr || name[0] == '\0') {
			continue;
		}
		HAnimClass *anim = mgr->Peek_HAnim(name);
		if (anim == nullptr) {
			continue;
		}
		W3DViewerAnimation entry;
		entry.name = name;
		entry.anim = anim;
		entry.num_frames = anim->Get_Num_Frames();
		entry.frame_rate = anim->Get_Frame_Rate();
		if (entry.num_frames <= 0) {
			entry.num_frames = 1;
		}
		if (entry.frame_rate <= 0.0f) {
			entry.frame_rate = 30.0f;
		}
		scene->animations.push_back(entry);
	}
	delete it;
}

static int SubObject_Draw_Priority(const W3DViewerSubObject &sub)
{
	/* Draw the main hull first; animated attachments must render after it or
	 * BOX07 overwrites their depth and they disappear. */
	if (sub.bone_index == 0) {
		if (sub.name.find("BOX07") != std::string::npos) {
			return 0;
		}
		return 1;
	}
	return 100 + sub.bone_index;
}

static void Sort_SubObjects_For_Draw(std::vector<W3DViewerSubObject> *sub_objects)
{
	std::stable_sort(
		sub_objects->begin(),
		sub_objects->end(),
		[](const W3DViewerSubObject &a, const W3DViewerSubObject &b) {
			const int pa = SubObject_Draw_Priority(a);
			const int pb = SubObject_Draw_Priority(b);
			if (pa != pb) {
				return pa < pb;
			}
			return a.bone_index < b.bone_index;
		});
}

static void Finalize_Scene_Bounds(W3DViewerScene *scene)
{
	if (scene->sub_objects.empty()) {
		return;
	}
	bool have_bounds = false;
	for (size_t i = 0; i < scene->sub_objects.size(); ++i) {
		const W3DViewerMesh &mesh = scene->sub_objects[i].mesh;
		if (mesh.vertex_count == 0) {
			continue;
		}
		if (!have_bounds) {
			memcpy(scene->static_mesh.bbox_min, mesh.bbox_min, sizeof(mesh.bbox_min));
			memcpy(scene->static_mesh.bbox_max, mesh.bbox_max, sizeof(mesh.bbox_max));
			have_bounds = true;
		} else {
			for (int axis = 0; axis < 3; ++axis) {
				scene->static_mesh.bbox_min[axis] =
					std::min(scene->static_mesh.bbox_min[axis], mesh.bbox_min[axis]);
				scene->static_mesh.bbox_max[axis] =
					std::max(scene->static_mesh.bbox_max[axis], mesh.bbox_max[axis]);
			}
		}
	}
	if (!have_bounds) {
		return;
	}
	scene->static_mesh.sph_center[0] =
		(scene->static_mesh.bbox_min[0] + scene->static_mesh.bbox_max[0]) * 0.5f;
	scene->static_mesh.sph_center[1] =
		(scene->static_mesh.bbox_min[1] + scene->static_mesh.bbox_max[1]) * 0.5f;
	scene->static_mesh.sph_center[2] =
		(scene->static_mesh.bbox_min[2] + scene->static_mesh.bbox_max[2]) * 0.5f;
	float max_dist_sq = 0.0f;
	for (size_t i = 0; i < scene->sub_objects.size(); ++i) {
		const W3DViewerMesh &mesh = scene->sub_objects[i].mesh;
		for (uint32_t v = 0; v < mesh.vertex_count; ++v) {
			float dx = mesh.positions[v * 3 + 0] - scene->static_mesh.sph_center[0];
			float dy = mesh.positions[v * 3 + 1] - scene->static_mesh.sph_center[1];
			float dz = mesh.positions[v * 3 + 2] - scene->static_mesh.sph_center[2];
			float dist_sq = dx * dx + dy * dy + dz * dz;
			if (dist_sq > max_dist_sq) {
				max_dist_sq = dist_sq;
			}
		}
	}
	scene->static_mesh.sph_radius = sqrtf(max_dist_sq);
}

} /* namespace */

void W3DViewer_Init_Assets()
{
	if (WW3DAssetManager::Get_Instance() == nullptr) {
		new WW3DAssetManager();
	}
}

void W3DViewer_Shutdown_Assets()
{
	WW3DAssetManager::Delete_This();
}

bool W3DViewer_Load_Scene(
	const char *model_path,
	const char *extra_anim_path,
	W3DViewerScene *scene)
{
	if (scene == nullptr || model_path == nullptr) {
		return false;
	}

	*scene = W3DViewerScene();

	if (!W3DViewer_Load_Mesh(model_path, &scene->static_mesh)) {
		return false;
	}

	W3DViewer_Init_Assets();
	WW3DAssetManager *mgr = WW3DAssetManager::Get_Instance();
	if (mgr == nullptr) {
		return true;
	}

	if (!mgr->Load_3D_Assets(model_path)) {
		fprintf(stderr, "w3dviewer: asset load warning for %s\n", model_path);
	}
	if (extra_anim_path != nullptr && extra_anim_path[0] != '\0') {
		if (!mgr->Load_3D_Assets(extra_anim_path)) {
			fprintf(stderr, "w3dviewer: failed to load anim file %s\n", extra_anim_path);
		}
	}

	std::vector<uint8_t> file_data;
	std::vector<HlodSubRef> hlod_refs;
	if (Read_File_Bytes(model_path, &file_data)) {
		Find_Hlod_In_Range(
			file_data.data(),
			(uint32_t)file_data.size(),
			0,
			&scene->hierarchy_name,
			&hlod_refs);
	}

	if (!scene->hierarchy_name.empty()) {
		scene->htree = mgr->Peek_HTree(scene->hierarchy_name.c_str());
	}
	if (scene->htree == nullptr) {
		AssetIterator *tree_it = mgr->Create_HTree_Iterator();
		if (tree_it != nullptr) {
			for (tree_it->First(); !tree_it->Is_Done(); tree_it->Next()) {
				const char *tree_name = tree_it->Current_Item_Name();
				if (tree_name != nullptr && tree_name[0] != '\0') {
					scene->htree = mgr->Peek_HTree(tree_name);
					if (scene->htree != nullptr) {
						scene->hierarchy_name = tree_name;
						break;
					}
				}
			}
			delete tree_it;
		}
	}

	std::vector<std::pair<std::string, W3DViewerMesh>> named_meshes;
	if (!W3DViewer_Load_Named_Meshes(model_path, &named_meshes)) {
		named_meshes.clear();
	}

	if (!hlod_refs.empty() && scene->htree != nullptr) {
		for (size_t i = 0; i < hlod_refs.size(); ++i) {
			W3DViewerMesh mesh;
			if (!Find_Mesh_By_Name(named_meshes, hlod_refs[i].name, &mesh)) {
				fprintf(
					stderr,
					"w3dviewer: HLOD sub-object mesh not found: %s\n",
					hlod_refs[i].name.c_str());
				continue;
			}
			W3DViewerSubObject sub;
			sub.name = hlod_refs[i].name;
			sub.bone_index = (int32_t)hlod_refs[i].bone_index;
			sub.mesh = mesh;
			scene->sub_objects.push_back(sub);
		}
	}

	Collect_Animations(scene);

	if (!scene->sub_objects.empty()) {
		Sort_SubObjects_For_Draw(&scene->sub_objects);
	}

	if (!scene->sub_objects.empty() && scene->htree != nullptr && !scene->animations.empty()) {
		scene->is_animated = true;
		Finalize_Scene_Bounds(scene);
		fprintf(
			stderr,
			"w3dviewer: animated scene — %zu sub-meshes, %zu animations, hierarchy=%s\n",
			scene->sub_objects.size(),
			scene->animations.size(),
			scene->hierarchy_name.c_str());
	} else {
		scene->is_animated = false;
		scene->sub_objects.clear();
		if (!scene->animations.empty()) {
			fprintf(
				stderr,
				"w3dviewer: %zu animation(s) loaded but no HLOD rig — static display\n",
				scene->animations.size());
		}
	}

	return true;
}

void W3DViewer_Mat4_From_Matrix3D(float out[16], const void *matrix3d)
{
	const Matrix3D &m = *reinterpret_cast<const Matrix3D *>(matrix3d);
	out[0] = m[0][0];
	out[1] = m[1][0];
	out[2] = m[2][0];
	out[3] = 0.0f;
	out[4] = m[0][1];
	out[5] = m[1][1];
	out[6] = m[2][1];
	out[7] = 0.0f;
	out[8] = m[0][2];
	out[9] = m[1][2];
	out[10] = m[2][2];
	out[11] = 0.0f;
	out[12] = m[0][3];
	out[13] = m[1][3];
	out[14] = m[2][3];
	out[15] = 1.0f;
}

void W3DViewer_Scene_Update(W3DViewerScene *scene, float delta_seconds)
{
	if (scene == nullptr || !scene->is_animated || scene->htree == nullptr) {
		return;
	}
	if (scene->animations.empty()) {
		Matrix3D root(true);
		scene->htree->Base_Update(root);
		return;
	}

	if (scene->current_anim < 0 || scene->current_anim >= (int32_t)scene->animations.size()) {
		scene->current_anim = 0;
	}

	W3DViewerAnimation &anim_entry = scene->animations[(size_t)scene->current_anim];
	HAnimClass *anim = anim_entry.anim;
	if (anim == nullptr) {
		return;
	}

	if (scene->anim_playing && delta_seconds > 0.0f) {
		scene->anim_time += delta_seconds * anim_entry.frame_rate;
	}
	const float max_frame = (float)(anim_entry.num_frames - 1);
	if (max_frame > 0.0f) {
		while (scene->anim_time > max_frame) {
			scene->anim_time -= max_frame + 1.0f;
		}
	} else {
		scene->anim_time = 0.0f;
	}

	Matrix3D root(true);
	scene->htree->Anim_Update(root, anim, scene->anim_time);
}

void W3DViewer_Scene_Select_Animation(W3DViewerScene *scene, int32_t index)
{
	if (scene == nullptr || scene->animations.empty()) {
		return;
	}
	if (index < 0) {
		index = 0;
	}
	if (index >= (int32_t)scene->animations.size()) {
		index = (int32_t)scene->animations.size() - 1;
	}
	scene->current_anim = index;
	scene->anim_time = 0.0f;
}

int32_t W3DViewer_Scene_Find_Animation(const W3DViewerScene &scene, const char *name)
{
	if (name == nullptr || name[0] == '\0') {
		return -1;
	}
	for (size_t i = 0; i < scene.animations.size(); ++i) {
		if (Name_Compare(scene.animations[i].name.c_str(), name) == 0) {
			return (int32_t)i;
		}
	}
	return -1;
}

void W3DViewer_Print_Scene_Info(const char *path, const W3DViewerScene &scene)
{
	W3DViewer_Print_Mesh_Info(path, scene.static_mesh);
	printf("  animated : %s\n", scene.is_animated ? "yes" : "no");
	if (!scene.hierarchy_name.empty()) {
		printf("  hierarchy: %s\n", scene.hierarchy_name.c_str());
	}
	printf("  sub-meshes: %zu\n", scene.sub_objects.size());
	printf("  animations: %zu\n", scene.animations.size());
	for (size_t i = 0; i < scene.animations.size(); ++i) {
		printf(
			"    [%zu] %s (%d frames, %.1f fps)\n",
			i,
			scene.animations[i].name.c_str(),
			scene.animations[i].num_frames,
			scene.animations[i].frame_rate);
	}
	for (size_t i = 0; i < scene.sub_objects.size(); ++i) {
		printf(
			"    bone %d -> %s (%u tris)\n",
			scene.sub_objects[i].bone_index,
			scene.sub_objects[i].name.c_str(),
			scene.sub_objects[i].mesh.tri_count);
	}
}
