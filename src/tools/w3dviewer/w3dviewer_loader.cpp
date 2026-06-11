#include "w3dviewer_loader.h"

#include "bittype.h"
#include "w3d_file.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
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

static bool Read_File(const char *path, std::vector<uint8_t> *out)
{
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "w3dviewer: cannot open %s\n", path);
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

static std::string Dirname(const char *path)
{
	std::string s(path);
	size_t slash = s.find_last_of("/\\");
	if (slash == std::string::npos) {
		return ".";
	}
	if (slash == 0) {
		return "/";
	}
	return s.substr(0, slash);
}

static std::string Read_Chunk_String(const uint8_t *body, uint32_t size)
{
	if (body == nullptr || size == 0) {
		return std::string();
	}
	size_t len = strnlen(reinterpret_cast<const char *>(body), size);
	return std::string(reinterpret_cast<const char *>(body), len);
}

static int Find_Texture_Index(const std::vector<std::string> &names, const std::string &name)
{
	for (size_t i = 0; i < names.size(); ++i) {
		if (strcasecmp(names[i].c_str(), name.c_str()) == 0) {
			return (int)i;
		}
	}
	return -1;
}

static int Add_Texture_Name(std::vector<std::string> *names, const std::string &name)
{
	if (name.empty()) {
		return -1;
	}
	int existing = Find_Texture_Index(*names, name);
	if (existing >= 0) {
		return existing;
	}
	names->push_back(name);
	return (int)names->size() - 1;
}

static std::string Make_Mesh_Full_Name(const char *container, const char *mesh)
{
	if (container == nullptr || container[0] == '\0') {
		return std::string(mesh != nullptr ? mesh : "");
	}
	if (mesh == nullptr || mesh[0] == '\0') {
		return std::string(container);
	}
	return std::string(container) + "." + mesh;
}

struct MeshPiece {
	std::string full_name;
	std::vector<float> positions;
	std::vector<float> normals;
	std::vector<float> uvs;
	std::vector<uint16_t> indices;
	std::vector<std::string> texture_names;
	std::vector<W3DViewerDrawBatch> draw_batches;
	uint32_t vertex_count = 0;
	uint32_t tri_count = 0;
	bool has_uvs = false;
	float bbox_min[3] = {0, 0, 0};
	float bbox_max[3] = {0, 0, 0};
	float sph_center[3] = {0, 0, 0};
	float sph_radius = 0.0f;
};

struct MaterialState {
	std::vector<std::string> textures;
	std::vector<uint32_t> tri_texture_ids;
	uint32_t single_texture_id = 0xffffffffu;
	bool has_single_texture = false;
	std::vector<W3dShaderStruct> shaders;
	std::vector<uint32_t> tri_shader_ids;
	uint32_t single_shader_id = 0u;
	bool has_single_shader = false;
};

static void Shader_Alpha_Flags(
	const W3dShaderStruct *shader,
	bool *alpha_blend,
	bool *alpha_test,
	bool *depth_write)
{
	*alpha_blend = false;
	*alpha_test = false;
	*depth_write = true;
	if (shader == nullptr) {
		return;
	}
	*alpha_blend = shader->SrcBlend != W3DSHADER_SRCBLENDFUNC_ONE ||
		shader->DestBlend != W3DSHADER_DESTBLENDFUNC_ZERO;
	*alpha_test = shader->AlphaTest == W3DSHADER_ALPHATEST_ENABLE;
	*depth_write = shader->DepthMask != W3DSHADER_DEPTHMASK_WRITE_DISABLE;
}

static const W3dShaderStruct *Resolve_Shader(const MaterialState &mat, uint32_t shader_id)
{
	if (mat.shaders.empty()) {
		return nullptr;
	}
	if (shader_id >= mat.shaders.size()) {
		return &mat.shaders[0];
	}
	return &mat.shaders[shader_id];
}

static void Expand_Bbox(MeshPiece *piece, float x, float y, float z)
{
	if (piece->vertex_count == 0) {
		piece->bbox_min[0] = piece->bbox_max[0] = x;
		piece->bbox_min[1] = piece->bbox_max[1] = y;
		piece->bbox_min[2] = piece->bbox_max[2] = z;
		return;
	}
	piece->bbox_min[0] = std::min(piece->bbox_min[0], x);
	piece->bbox_min[1] = std::min(piece->bbox_min[1], y);
	piece->bbox_min[2] = std::min(piece->bbox_min[2], z);
	piece->bbox_max[0] = std::max(piece->bbox_max[0], x);
	piece->bbox_max[1] = std::max(piece->bbox_max[1], y);
	piece->bbox_max[2] = std::max(piece->bbox_max[2], z);
}

static void Parse_Texture_Chunk(const W3dChunkView &chunk, MaterialState *mat)
{
	const uint8_t *ptr = chunk.body;
	const uint8_t *end = chunk.body + chunk.body_size;
	while (ptr < end) {
		W3dChunkView sub = {};
		if (!Read_Chunk(ptr, end, &sub)) {
			break;
		}
		if (sub.type == W3D_CHUNK_TEXTURE_NAME) {
			std::string name = Read_Chunk_String(sub.body, sub.body_size);
			if (!name.empty()) {
				mat->textures.push_back(name);
			}
		}
		ptr += sizeof(W3dChunkHeader) + sub.body_size;
	}
}

static void Parse_Shader_Ids(const W3dChunkView &chunk, MaterialState *mat, uint32_t tri_count)
{
	if (chunk.body_size == sizeof(uint32_t)) {
		memcpy(&mat->single_shader_id, chunk.body, sizeof(uint32_t));
		mat->has_single_shader = true;
		mat->tri_shader_ids.clear();
		return;
	}

	uint32_t count = chunk.body_size / sizeof(uint32_t);
	mat->tri_shader_ids.resize(count);
	memcpy(mat->tri_shader_ids.data(), chunk.body, chunk.body_size);
	mat->has_single_shader = false;
	if (tri_count > 0 && count != tri_count) {
		fprintf(
			stderr,
			"w3dviewer: shader id count %u != tri count %u\n",
			count,
			tri_count);
	}
}

static void Parse_Shaders_Chunk(const W3dChunkView &chunk, MaterialState *mat)
{
	if (chunk.body_size < sizeof(W3dShaderStruct)) {
		return;
	}
	uint32_t count = chunk.body_size / (uint32_t)sizeof(W3dShaderStruct);
	mat->shaders.resize(count);
	memcpy(mat->shaders.data(), chunk.body, count * sizeof(W3dShaderStruct));
}

static void Parse_Texture_Ids(const W3dChunkView &chunk, MaterialState *mat, uint32_t tri_count)
{
	if (chunk.body_size == sizeof(uint32_t)) {
		memcpy(&mat->single_texture_id, chunk.body, sizeof(uint32_t));
		mat->has_single_texture = true;
		mat->tri_texture_ids.clear();
		return;
	}

	uint32_t count = chunk.body_size / sizeof(uint32_t);
	mat->tri_texture_ids.resize(count);
	memcpy(mat->tri_texture_ids.data(), chunk.body, chunk.body_size);
	mat->has_single_texture = false;
	if (tri_count > 0 && count != tri_count) {
		fprintf(
			stderr,
			"w3dviewer: texture id count %u != tri count %u\n",
			count,
			tri_count);
	}
}

static void Parse_Material_Subtree(
	const W3dChunkView &chunk,
	MeshPiece *piece,
	MaterialState *mat,
	std::vector<W3dTexCoordStruct> *texcoords,
	int pass,
	int stage);

static void Parse_Material_Range(
	const uint8_t *data,
	uint32_t size,
	MeshPiece *piece,
	MaterialState *mat,
	std::vector<W3dTexCoordStruct> *texcoords,
	int pass,
	int stage)
{
	const uint8_t *ptr = data;
	const uint8_t *end = data + size;
	int cur_pass = pass;
	int cur_stage = stage;

	while (ptr < end) {
		W3dChunkView chunk = {};
		if (!Read_Chunk(ptr, end, &chunk)) {
			break;
		}

		if (chunk.type == W3D_CHUNK_MATERIAL_PASS) {
			cur_stage = 0;
			Parse_Material_Range(chunk.body, chunk.body_size, piece, mat, texcoords, cur_pass, 0);
			cur_pass++;
		} else if (chunk.type == W3D_CHUNK_TEXTURE_STAGE) {
			Parse_Material_Range(chunk.body, chunk.body_size, piece, mat, texcoords, cur_pass, cur_stage);
			cur_stage++;
		} else {
			Parse_Material_Subtree(chunk, piece, mat, texcoords, cur_pass, cur_stage);
		}

		ptr += sizeof(W3dChunkHeader) + chunk.body_size;
	}
}

static void Parse_Material_Subtree(
	const W3dChunkView &chunk,
	MeshPiece *piece,
	MaterialState *mat,
	std::vector<W3dTexCoordStruct> *texcoords,
	int pass,
	int stage)
{
	(void)pass;
	(void)stage;

	switch (chunk.type) {
	case W3D_CHUNK_TEXTURE:
		Parse_Texture_Chunk(chunk, mat);
		return;

	case W3D_CHUNK_TEXTURE_IDS:
		Parse_Texture_Ids(chunk, mat, piece->tri_count);
		return;

	case W3D_CHUNK_SHADER_IDS:
		Parse_Shader_Ids(chunk, mat, piece->tri_count);
		return;

	case W3D_CHUNK_SHADERS:
		Parse_Shaders_Chunk(chunk, mat);
		return;

	case W3D_CHUNK_MATERIAL_INFO:
		if (chunk.has_subchunks) {
			Parse_Material_Range(chunk.body, chunk.body_size, piece, mat, texcoords, pass, stage);
		}
		return;

	case W3D_CHUNK_STAGE_TEXCOORDS:
	case W3D_CHUNK_TEXCOORDS:
		if (chunk.body_size % sizeof(W3dTexCoordStruct) == 0) {
			uint32_t count = chunk.body_size / (uint32_t)sizeof(W3dTexCoordStruct);
			if (texcoords->empty() || stage == 0) {
				texcoords->resize(count);
				memcpy(texcoords->data(), chunk.body, chunk.body_size);
			}
		}
		return;

	case W3D_CHUNK_PER_FACE_TEXCOORD_IDS:
		return;

	case W3D_CHUNK_PRELIT_UNLIT:
	case W3D_CHUNK_PRELIT_VERTEX:
	case W3D_CHUNK_PRELIT_LIGHTMAP_MULTI_PASS:
	case W3D_CHUNK_PRELIT_LIGHTMAP_MULTI_TEXTURE:
	case W3D_CHUNK_TEXTURES:
	case W3D_CHUNK_MATERIAL_PASS:
	case W3D_CHUNK_TEXTURE_STAGE:
	case W3D_CHUNK_VERTEX_MATERIALS:
		if (chunk.has_subchunks) {
			Parse_Material_Range(chunk.body, chunk.body_size, piece, mat, texcoords, pass, stage);
		}
		return;

	default:
		return;
	}
}

static uint32_t Resolve_Tri_Texture_Id(const MaterialState &mat, uint32_t tri_index)
{
	if (mat.textures.empty()) {
		return 0xffffffffu;
	}
	if (mat.has_single_texture) {
		return mat.single_texture_id;
	}
	if (tri_index < mat.tri_texture_ids.size()) {
		return mat.tri_texture_ids[tri_index];
	}
	return 0u;
}

static uint32_t Resolve_Tri_Shader_Id(const MaterialState &mat, uint32_t tri_index)
{
	if (mat.shaders.empty()) {
		return 0xffffffffu;
	}
	if (mat.has_single_shader) {
		return mat.single_shader_id;
	}
	if (tri_index < mat.tri_shader_ids.size()) {
		return mat.tri_shader_ids[tri_index];
	}
	return 0u;
}

struct BatchBucketKey {
	uint32_t tex_id = 0xffffffffu;
	uint32_t shader_id = 0xffffffffu;

	bool operator<(const BatchBucketKey &other) const
	{
		if (tex_id != other.tex_id) {
			return tex_id < other.tex_id;
		}
		return shader_id < other.shader_id;
	}
};

static void Build_Draw_Batches(MeshPiece *piece, const MaterialState &mat)
{
	piece->texture_names = mat.textures;
	piece->draw_batches.clear();

	if (piece->tri_count == 0) {
		return;
	}

	std::map<BatchBucketKey, std::vector<uint16_t>> buckets;
	for (uint32_t t = 0; t < piece->tri_count; ++t) {
		uint32_t tex_id = Resolve_Tri_Texture_Id(mat, t);
		if (tex_id == 0xffffffffu || tex_id >= mat.textures.size()) {
			tex_id = 0xffffffffu;
		}
		uint32_t shader_id = Resolve_Tri_Shader_Id(mat, t);
		if (shader_id == 0xffffffffu || shader_id >= mat.shaders.size()) {
			shader_id = 0xffffffffu;
		}
		BatchBucketKey key;
		key.tex_id = tex_id;
		key.shader_id = shader_id;
		std::vector<uint16_t> &bucket = buckets[key];
		const size_t base = (size_t)t * 3;
		bucket.push_back(piece->indices[base + 0]);
		bucket.push_back(piece->indices[base + 1]);
		bucket.push_back(piece->indices[base + 2]);
	}

	for (std::map<BatchBucketKey, std::vector<uint16_t>>::iterator it = buckets.begin();
		 it != buckets.end();
		 ++it) {
		W3DViewerDrawBatch batch;
		if (it->first.tex_id == 0xffffffffu) {
			batch.texture_index = -1;
		} else {
			batch.texture_index = (int32_t)it->first.tex_id;
		}
		const W3dShaderStruct *shader = nullptr;
		if (it->first.shader_id != 0xffffffffu) {
			shader = Resolve_Shader(mat, it->first.shader_id);
		}
		Shader_Alpha_Flags(shader, &batch.alpha_blend, &batch.alpha_test, &batch.depth_write);
		batch.indices.swap(it->second);
		piece->draw_batches.push_back(batch);
	}

	if (piece->draw_batches.empty()) {
		W3DViewerDrawBatch batch;
		batch.texture_index = -1;
		batch.indices = piece->indices;
		piece->draw_batches.push_back(batch);
	}
}

static void Parse_Mesh(const uint8_t *data, uint32_t size, MeshPiece *piece);

static void Parse_Chunk_Range(
	const uint8_t *data,
	uint32_t size,
	int depth,
	std::vector<MeshPiece> *pieces)
{
	const uint8_t *ptr = data;
	const uint8_t *end = data + size;
	while (ptr < end) {
		W3dChunkView chunk = {};
		if (!Read_Chunk(ptr, end, &chunk)) {
			break;
		}

		if (chunk.type == W3D_CHUNK_MESH) {
			MeshPiece piece = {};
			Parse_Mesh(chunk.body, chunk.body_size, &piece);
			if (piece.vertex_count > 0 && piece.tri_count > 0) {
				pieces->push_back(piece);
			}
		} else if (chunk.has_subchunks && depth < 8) {
			Parse_Chunk_Range(chunk.body, chunk.body_size, depth + 1, pieces);
		}

		ptr += sizeof(W3dChunkHeader) + chunk.body_size;
	}
}

static void Parse_Mesh(const uint8_t *data, uint32_t size, MeshPiece *piece)
{
	const uint8_t *ptr = data;
	const uint8_t *end = data + size;

	W3dMeshHeader3Struct header = {};
	bool have_header = false;
	std::vector<W3dVectorStruct> verts;
	std::vector<W3dVectorStruct> norms;
	std::vector<W3dTexCoordStruct> texcoords;
	std::vector<W3dTriStruct> tris;
	MaterialState mat;

	while (ptr < end) {
		W3dChunkView chunk = {};
		if (!Read_Chunk(ptr, end, &chunk)) {
			break;
		}

		switch (chunk.type) {
		case W3D_CHUNK_MESH_HEADER3:
			if (chunk.body_size >= sizeof(W3dMeshHeader3Struct)) {
				memcpy(&header, chunk.body, sizeof(header));
				have_header = true;
				piece->full_name = Make_Mesh_Full_Name(header.ContainerName, header.MeshName);
				piece->sph_center[0] = header.SphCenter.X;
				piece->sph_center[1] = header.SphCenter.Y;
				piece->sph_center[2] = header.SphCenter.Z;
				piece->sph_radius = header.SphRadius;
				piece->bbox_min[0] = header.Min.X;
				piece->bbox_min[1] = header.Min.Y;
				piece->bbox_min[2] = header.Min.Z;
				piece->bbox_max[0] = header.Max.X;
				piece->bbox_max[1] = header.Max.Y;
				piece->bbox_max[2] = header.Max.Z;
			}
			break;

		case W3D_CHUNK_VERTICES:
			if (chunk.body_size % sizeof(W3dVectorStruct) == 0) {
				uint32_t count = chunk.body_size / (uint32_t)sizeof(W3dVectorStruct);
				verts.resize(count);
				memcpy(verts.data(), chunk.body, chunk.body_size);
			}
			break;

		case W3D_CHUNK_VERTEX_NORMALS:
			if (chunk.body_size % sizeof(W3dVectorStruct) == 0) {
				uint32_t count = chunk.body_size / (uint32_t)sizeof(W3dVectorStruct);
				norms.resize(count);
				memcpy(norms.data(), chunk.body, chunk.body_size);
			}
			break;

		case W3D_CHUNK_TRIANGLES:
			if (chunk.body_size % sizeof(W3dTriStruct) == 0) {
				uint32_t count = chunk.body_size / (uint32_t)sizeof(W3dTriStruct);
				tris.resize(count);
				memcpy(tris.data(), chunk.body, chunk.body_size);
			}
			break;

		case W3D_CHUNK_STAGE_TEXCOORDS:
		case W3D_CHUNK_TEXCOORDS:
			if (chunk.body_size % sizeof(W3dTexCoordStruct) == 0) {
				uint32_t count = chunk.body_size / (uint32_t)sizeof(W3dTexCoordStruct);
				if (texcoords.empty()) {
					texcoords.resize(count);
					memcpy(texcoords.data(), chunk.body, chunk.body_size);
				}
			}
			break;

		case W3D_CHUNK_PRELIT_UNLIT:
		case W3D_CHUNK_PRELIT_VERTEX:
		case W3D_CHUNK_PRELIT_LIGHTMAP_MULTI_PASS:
		case W3D_CHUNK_PRELIT_LIGHTMAP_MULTI_TEXTURE:
		case W3D_CHUNK_TEXTURES:
		case W3D_CHUNK_MATERIAL_PASS:
		case W3D_CHUNK_MATERIAL_INFO:
		case W3D_CHUNK_VERTEX_MATERIALS:
		case W3D_CHUNK_SHADERS:
		case W3D_CHUNK_TEXTURE:
			Parse_Material_Subtree(chunk, piece, &mat, &texcoords, 0, 0);
			break;

		default:
			break;
		}

		ptr += sizeof(W3dChunkHeader) + chunk.body_size;
	}

	if (verts.empty() || tris.empty()) {
		return;
	}

	piece->vertex_count = (uint32_t)verts.size();
	piece->tri_count = (uint32_t)tris.size();
	piece->positions.resize(verts.size() * 3);
	for (size_t i = 0; i < verts.size(); ++i) {
		piece->positions[i * 3 + 0] = verts[i].X;
		piece->positions[i * 3 + 1] = verts[i].Y;
		piece->positions[i * 3 + 2] = verts[i].Z;
		if (!have_header) {
			Expand_Bbox(piece, verts[i].X, verts[i].Y, verts[i].Z);
		}
	}

	piece->normals.resize(verts.size() * 3, 0.0f);
	if (!norms.empty()) {
		for (size_t i = 0; i < norms.size() && i < verts.size(); ++i) {
			piece->normals[i * 3 + 0] = norms[i].X;
			piece->normals[i * 3 + 1] = norms[i].Y;
			piece->normals[i * 3 + 2] = norms[i].Z;
		}
	}

	if (!texcoords.empty()) {
		piece->has_uvs = true;
		piece->uvs.resize(verts.size() * 2, 0.0f);
		for (size_t i = 0; i < texcoords.size() && i < verts.size(); ++i) {
			/* W3D/D3D UVs: v=0 top; Vulkan sampler uses the same convention. */
			piece->uvs[i * 2 + 0] = texcoords[i].U;
			piece->uvs[i * 2 + 1] = texcoords[i].V;
		}
	}

	piece->indices.resize(tris.size() * 3);
	for (size_t i = 0; i < tris.size(); ++i) {
		piece->indices[i * 3 + 0] = (uint16_t)tris[i].Vindex[0];
		piece->indices[i * 3 + 1] = (uint16_t)tris[i].Vindex[1];
		piece->indices[i * 3 + 2] = (uint16_t)tris[i].Vindex[2];
	}

	Build_Draw_Batches(piece, mat);

	if (!have_header && piece->vertex_count > 0) {
		piece->sph_center[0] = (piece->bbox_min[0] + piece->bbox_max[0]) * 0.5f;
		piece->sph_center[1] = (piece->bbox_min[1] + piece->bbox_max[1]) * 0.5f;
		piece->sph_center[2] = (piece->bbox_min[2] + piece->bbox_max[2]) * 0.5f;
		float dx = piece->bbox_max[0] - piece->bbox_min[0];
		float dy = piece->bbox_max[1] - piece->bbox_min[1];
		float dz = piece->bbox_max[2] - piece->bbox_min[2];
		piece->sph_radius = 0.5f * sqrtf(dx * dx + dy * dy + dz * dz);
	}
}

static void Append_Piece(W3DViewerMesh *mesh, const MeshPiece &piece)
{
	const uint32_t base = mesh->vertex_count;
	mesh->positions.insert(mesh->positions.end(), piece.positions.begin(), piece.positions.end());
	mesh->normals.insert(mesh->normals.end(), piece.normals.begin(), piece.normals.end());

	if (piece.has_uvs) {
		if (!mesh->has_uvs) {
			mesh->uvs.resize(mesh->vertex_count * 2, 0.0f);
			mesh->has_uvs = true;
		} else if (mesh->uvs.size() < mesh->vertex_count * 2) {
			mesh->uvs.resize(mesh->vertex_count * 2, 0.0f);
		}
		mesh->uvs.resize((mesh->vertex_count + piece.vertex_count) * 2, 0.0f);
		memcpy(
			mesh->uvs.data() + mesh->vertex_count * 2,
			piece.uvs.data(),
			piece.uvs.size() * sizeof(float));
	} else if (mesh->has_uvs) {
		mesh->uvs.resize((mesh->vertex_count + piece.vertex_count) * 2, 0.0f);
	}

	for (size_t b = 0; b < piece.draw_batches.size(); ++b) {
		const W3DViewerDrawBatch &src = piece.draw_batches[b];
		W3DViewerDrawBatch dst;
		if (src.texture_index >= 0 &&
				src.texture_index < (int32_t)piece.texture_names.size()) {
			dst.texture_index = Add_Texture_Name(
				&mesh->texture_names,
				piece.texture_names[(size_t)src.texture_index]);
		} else {
			dst.texture_index = -1;
		}
		dst.alpha_blend = src.alpha_blend;
		dst.alpha_test = src.alpha_test;
		dst.depth_write = src.depth_write;
		dst.indices.resize(src.indices.size());
		for (size_t i = 0; i < src.indices.size(); ++i) {
			dst.indices[i] = (uint16_t)(base + src.indices[i]);
		}
		mesh->draw_batches.push_back(dst);
	}

	if (mesh->vertex_count == 0) {
		memcpy(mesh->bbox_min, piece.bbox_min, sizeof(mesh->bbox_min));
		memcpy(mesh->bbox_max, piece.bbox_max, sizeof(mesh->bbox_max));
	} else {
		for (int axis = 0; axis < 3; ++axis) {
			mesh->bbox_min[axis] = std::min(mesh->bbox_min[axis], piece.bbox_min[axis]);
			mesh->bbox_max[axis] = std::max(mesh->bbox_max[axis], piece.bbox_max[axis]);
		}
	}

	mesh->vertex_count += piece.vertex_count;
	mesh->tri_count += piece.tri_count;
}

static void Finalize_Bounds(W3DViewerMesh *mesh)
{
	if (mesh->vertex_count == 0) {
		return;
	}
	mesh->sph_center[0] = (mesh->bbox_min[0] + mesh->bbox_max[0]) * 0.5f;
	mesh->sph_center[1] = (mesh->bbox_min[1] + mesh->bbox_max[1]) * 0.5f;
	mesh->sph_center[2] = (mesh->bbox_min[2] + mesh->bbox_max[2]) * 0.5f;
	float max_dist_sq = 0.0f;
	for (uint32_t i = 0; i < mesh->vertex_count; ++i) {
		float dx = mesh->positions[i * 3 + 0] - mesh->sph_center[0];
		float dy = mesh->positions[i * 3 + 1] - mesh->sph_center[1];
		float dz = mesh->positions[i * 3 + 2] - mesh->sph_center[2];
		float dist_sq = dx * dx + dy * dy + dz * dz;
		if (dist_sq > max_dist_sq) {
			max_dist_sq = dist_sq;
		}
	}
	mesh->sph_radius = sqrtf(max_dist_sq);
}

static void Merge_Draw_Batches(W3DViewerMesh *mesh)
{
	if (mesh->draw_batches.empty()) {
		W3DViewerDrawBatch batch;
		batch.texture_index = -1;
		batch.indices = mesh->indices;
		mesh->draw_batches.push_back(batch);
		return;
	}

	std::vector<W3DViewerDrawBatch> merged;
	for (size_t i = 0; i < mesh->draw_batches.size(); ++i) {
		const W3DViewerDrawBatch &src = mesh->draw_batches[i];
		bool found = false;
		for (size_t j = 0; j < merged.size(); ++j) {
			if (merged[j].texture_index == src.texture_index &&
				merged[j].alpha_blend == src.alpha_blend &&
				merged[j].alpha_test == src.alpha_test &&
				merged[j].depth_write == src.depth_write) {
				merged[j].indices.insert(
					merged[j].indices.end(),
					src.indices.begin(),
					src.indices.end());
				found = true;
				break;
			}
		}
		if (!found) {
			merged.push_back(src);
		}
	}
	mesh->draw_batches.swap(merged);
}

} /* namespace */

bool W3DViewer_Load_Named_Meshes(
	const char *path,
	std::vector<std::pair<std::string, W3DViewerMesh>> *out_meshes)
{
	std::vector<uint8_t> file_data;
	if (!Read_File(path, &file_data)) {
		return false;
	}

	out_meshes->clear();
	std::vector<MeshPiece> pieces;
	Parse_Chunk_Range(file_data.data(), (uint32_t)file_data.size(), 0, &pieces);

	const std::string search_dir = Dirname(path);
	for (size_t i = 0; i < pieces.size(); ++i) {
		if (pieces[i].vertex_count == 0 || pieces[i].tri_count == 0) {
			continue;
		}
		W3DViewerMesh mesh;
		mesh.texture_search_dir = search_dir;
		mesh.has_uvs = pieces[i].has_uvs;
		mesh.vertex_count = pieces[i].vertex_count;
		mesh.tri_count = pieces[i].tri_count;
		mesh.positions = pieces[i].positions;
		mesh.normals = pieces[i].normals;
		mesh.uvs = pieces[i].uvs;
		mesh.texture_names = pieces[i].texture_names;
		mesh.draw_batches = pieces[i].draw_batches;
		memcpy(mesh.bbox_min, pieces[i].bbox_min, sizeof(mesh.bbox_min));
		memcpy(mesh.bbox_max, pieces[i].bbox_max, sizeof(mesh.bbox_max));
		memcpy(mesh.sph_center, pieces[i].sph_center, sizeof(mesh.sph_center));
		mesh.sph_radius = pieces[i].sph_radius;
		for (size_t b = 0; b < mesh.draw_batches.size(); ++b) {
			mesh.indices.insert(
				mesh.indices.end(),
				mesh.draw_batches[b].indices.begin(),
				mesh.draw_batches[b].indices.end());
		}
		std::string name = pieces[i].full_name;
		if (name.empty()) {
			char fallback[32];
			snprintf(fallback, sizeof(fallback), "mesh_%zu", i);
			name = fallback;
		}
		out_meshes->push_back(std::make_pair(name, mesh));
	}
	return !out_meshes->empty();
}

bool W3DViewer_Load_Mesh(const char *path, W3DViewerMesh *out_mesh)
{
	std::vector<uint8_t> file_data;
	if (!Read_File(path, &file_data)) {
		return false;
	}

	out_mesh->positions.clear();
	out_mesh->normals.clear();
	out_mesh->uvs.clear();
	out_mesh->indices.clear();
	out_mesh->texture_names.clear();
	out_mesh->draw_batches.clear();
	out_mesh->texture_search_dir.clear();
	out_mesh->vertex_count = 0;
	out_mesh->tri_count = 0;
	out_mesh->has_uvs = false;
	memset(out_mesh->bbox_min, 0, sizeof(out_mesh->bbox_min));
	memset(out_mesh->bbox_max, 0, sizeof(out_mesh->bbox_max));
	memset(out_mesh->sph_center, 0, sizeof(out_mesh->sph_center));
	out_mesh->sph_radius = 0.0f;
	out_mesh->texture_search_dir = Dirname(path);

	std::vector<MeshPiece> pieces;
	Parse_Chunk_Range(file_data.data(), (uint32_t)file_data.size(), 0, &pieces);
	for (size_t i = 0; i < pieces.size(); ++i) {
		Append_Piece(out_mesh, pieces[i]);
	}
	Merge_Draw_Batches(out_mesh);
	out_mesh->indices.clear();
	for (size_t i = 0; i < out_mesh->draw_batches.size(); ++i) {
		const W3DViewerDrawBatch &batch = out_mesh->draw_batches[i];
		out_mesh->indices.insert(
			out_mesh->indices.end(),
			batch.indices.begin(),
			batch.indices.end());
	}
	Finalize_Bounds(out_mesh);

	if (out_mesh->vertex_count == 0 || out_mesh->tri_count == 0) {
		fprintf(stderr, "w3dviewer: no mesh geometry found in %s\n", path);
		return false;
	}
	return true;
}

void W3DViewer_Print_Mesh_Info(const char *path, const W3DViewerMesh &mesh)
{
	printf(
		"%s\n"
		"  vertices : %u\n"
		"  triangles: %u\n"
		"  uvs      : %s\n"
		"  textures : %zu\n"
		"  batches  : %zu\n"
		"  bbox     : [%.3f, %.3f, %.3f] .. [%.3f, %.3f, %.3f]\n"
		"  sphere   : center (%.3f, %.3f, %.3f) radius %.3f\n",
		path,
		mesh.vertex_count,
		mesh.tri_count,
		mesh.has_uvs ? "yes" : "no",
		mesh.texture_names.size(),
		mesh.draw_batches.size(),
		mesh.bbox_min[0], mesh.bbox_min[1], mesh.bbox_min[2],
		mesh.bbox_max[0], mesh.bbox_max[1], mesh.bbox_max[2],
		mesh.sph_center[0], mesh.sph_center[1], mesh.sph_center[2],
		mesh.sph_radius);

	for (size_t i = 0; i < mesh.texture_names.size(); ++i) {
		printf("    [%zu] %s\n", i, mesh.texture_names[i].c_str());
	}
}
