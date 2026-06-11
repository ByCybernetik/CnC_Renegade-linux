#ifndef W3DVIEWER_LOADER_H
#define W3DVIEWER_LOADER_H

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

struct W3DViewerDrawBatch {
	int32_t texture_index = -1;
	std::vector<uint16_t> indices;
	bool alpha_blend = false;
	bool alpha_test = false;
	bool depth_write = true;
};

struct W3DViewerMesh {
	std::vector<float> positions;
	std::vector<float> normals;
	std::vector<float> uvs;
	std::vector<uint16_t> indices;
	std::vector<std::string> texture_names;
	std::vector<W3DViewerDrawBatch> draw_batches;
	std::string texture_search_dir;
	uint32_t vertex_count = 0;
	uint32_t tri_count = 0;
	float bbox_min[3] = {0, 0, 0};
	float bbox_max[3] = {0, 0, 0};
	float sph_center[3] = {0, 0, 0};
	float sph_radius = 0.0f;
	bool has_uvs = false;
};

bool W3DViewer_Load_Mesh(const char *path, W3DViewerMesh *out_mesh);
bool W3DViewer_Load_Named_Meshes(
	const char *path,
	std::vector<std::pair<std::string, W3DViewerMesh>> *out_meshes);
void W3DViewer_Print_Mesh_Info(const char *path, const W3DViewerMesh &mesh);

#endif
