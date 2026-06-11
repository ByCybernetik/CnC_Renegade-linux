#include "w3dviewer_texture.h"

#include "../ww3d2_vulkan/stb_texture.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <dirent.h>
#include <strings.h>
#endif

#if defined(_WIN32)
#define w3dviewer_stricmp _stricmp
#else
#define w3dviewer_stricmp strcasecmp
#endif

namespace w3dviewer {

GpuTexture::~GpuTexture()
{
	texture_.Destroy();
}

bool GpuTexture::Create_White_Fallback()
{
	texture_.Destroy();
	return texture_.Create_Solid(255, 255, 255, 255);
}

bool GpuTexture::Load_From_File(const std::string &path)
{
	texture_.Destroy();

	ww3d_vulkan::StbLoadedTexture loaded;
	if (!ww3d_vulkan::Stb_Load_Texture(path.c_str(), 0, true, true, false, &loaded)) {
		fprintf(stderr, "w3dviewer: failed to load texture %s\n", path.c_str());
		return false;
	}

	const VkSamplerAddressMode wrap = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	bool ok = false;
	if (loaded.compressed) {
		ok = texture_.Create_From_Compressed(
			loaded.format,
			loaded.width,
			loaded.height,
			loaded.mip_levels,
			loaded.pixels.data(),
			loaded.pixels.size(),
			wrap,
			wrap);
	} else {
		ok = texture_.Create_From_Rgba8(
			loaded.pixels.data(),
			loaded.width,
			loaded.height,
			wrap,
			wrap);
	}
	if (!ok) {
		fprintf(stderr, "w3dviewer: Vulkan upload failed for %s\n", path.c_str());
		return false;
	}
	texture_.Set_Layout_Shader_Read_Only();
	return true;
}

static bool File_Exists(const std::string &path)
{
	FILE *f = fopen(path.c_str(), "rb");
	if (f == nullptr) {
		return false;
	}
	fclose(f);
	return true;
}

static std::string Join_Path(const std::string &dir, const std::string &file)
{
	if (dir.empty()) {
		return file;
	}
	if (dir.back() == '/' || dir.back() == '\\') {
		return dir + file;
	}
	return dir + "/" + file;
}

static std::string Base_Name(const std::string &path)
{
	size_t slash = path.find_last_of("/\\");
	return (slash == std::string::npos) ? path : path.substr(slash + 1);
}

static std::string Strip_Extension(const std::string &filename)
{
	size_t slash = filename.find_last_of("/\\");
	const size_t name_start = (slash == std::string::npos) ? 0 : slash + 1;
	size_t dot = filename.find_last_of('.');
	if (dot == std::string::npos || dot < name_start) {
		return filename;
	}
	return filename.substr(0, dot);
}

static bool Is_Texture_Extension(const std::string &ext)
{
	return w3dviewer_stricmp(ext.c_str(), ".png") == 0 ||
		w3dviewer_stricmp(ext.c_str(), ".tga") == 0 ||
		w3dviewer_stricmp(ext.c_str(), ".dds") == 0;
}

static bool Find_Case_Insensitive_In_Dir(
	const std::string &dir,
	const std::string &want_base,
	std::string *out_path)
{
	if (dir.empty()) {
		return false;
	}

#if defined(_WIN32)
	WIN32_FIND_DATAA fd;
	std::string pattern = Join_Path(dir, "*");
	HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) {
		return false;
	}
	do {
		if (fd.cFileName[0] == '.') {
			continue;
		}
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			continue;
		}
		std::string name = fd.cFileName;
		if (w3dviewer_stricmp(Strip_Extension(name).c_str(), want_base.c_str()) != 0) {
			continue;
		}
		size_t dot = name.find_last_of('.');
		if (dot == std::string::npos) {
			continue;
		}
		if (!Is_Texture_Extension(name.substr(dot))) {
			continue;
		}
		*out_path = Join_Path(dir, name);
		FindClose(h);
		return true;
	} while (FindNextFileA(h, &fd));
	FindClose(h);
	return false;
#else
	DIR *d = opendir(dir.c_str());
	if (d == nullptr) {
		return false;
	}
	for (dirent *ent = readdir(d); ent != nullptr; ent = readdir(d)) {
		if (ent->d_name[0] == '.') {
			continue;
		}
		std::string name = ent->d_name;
		if (w3dviewer_stricmp(Strip_Extension(name).c_str(), want_base.c_str()) != 0) {
			continue;
		}
		size_t dot = name.find_last_of('.');
		if (dot == std::string::npos) {
			continue;
		}
		if (!Is_Texture_Extension(name.substr(dot))) {
			continue;
		}
		*out_path = Join_Path(dir, name);
		closedir(d);
		return true;
	}
	closedir(d);
	return false;
#endif
}

static void Append_Search_Dir(std::vector<std::string> *dirs, const std::string &dir)
{
	if (dir.empty()) {
		return;
	}
	for (size_t i = 0; i < dirs->size(); ++i) {
		if (w3dviewer_stricmp(dirs->at(i).c_str(), dir.c_str()) == 0) {
			return;
		}
	}
	dirs->push_back(dir);
}

static bool Try_Path_List(const std::string *paths, int count, std::string *out_path)
{
	for (int i = 0; i < count; ++i) {
		if (File_Exists(paths[i])) {
			*out_path = paths[i];
			return true;
		}
	}
	return false;
}

bool Resolve_Texture_Path(
	const std::string &filename,
	const std::string &search_dir,
	const std::string &extra_dir,
	std::string *out_path)
{
	if (filename.empty()) {
		return false;
	}

	std::string candidates[3];
	int count = 0;
	candidates[count++] = filename;
	if (!search_dir.empty()) {
		candidates[count++] = Join_Path(search_dir, filename);
	}
	if (!extra_dir.empty()) {
		candidates[count++] = Join_Path(extra_dir, filename);
	}
	if (Try_Path_List(candidates, count, out_path)) {
		return true;
	}

	static const char *kExts[] = {".png", ".tga", ".dds"};
	const std::string base = Strip_Extension(filename);
	for (size_t e = 0; e < sizeof(kExts) / sizeof(kExts[0]); ++e) {
		const std::string alt = base + kExts[e];
		if (w3dviewer_stricmp(alt.c_str(), filename.c_str()) == 0) {
			continue;
		}
		count = 0;
		candidates[count++] = alt;
		if (!search_dir.empty()) {
			candidates[count++] = Join_Path(search_dir, alt);
		}
		if (!extra_dir.empty()) {
			candidates[count++] = Join_Path(extra_dir, alt);
		}
		if (Try_Path_List(candidates, count, out_path)) {
			return true;
		}
	}

	const std::string want_base = Strip_Extension(Base_Name(filename));
	std::vector<std::string> dirs;
	Append_Search_Dir(&dirs, search_dir);
	Append_Search_Dir(&dirs, extra_dir);
	size_t slash = filename.find_last_of("/\\");
	if (slash != std::string::npos) {
		Append_Search_Dir(&dirs, filename.substr(0, slash));
	}
	for (size_t i = 0; i < dirs.size(); ++i) {
		if (Find_Case_Insensitive_In_Dir(dirs[i], want_base, out_path)) {
			return true;
		}
	}

	return false;
}

} /* namespace w3dviewer */
