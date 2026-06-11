#include "vk_shader.h"
#include "vk_check.h"
#include "vk_context.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#if defined(__linux__)
#include <limits.h>
#include <unistd.h>
#endif

namespace ww3d_vulkan {

namespace {

static const uint32_t kSpirvMagic = 0x07230203u;

bool File_Exists(const char *path)
{
	if (path == nullptr || path[0] == '\0') {
		return false;
	}
	std::ifstream file(path, std::ios::binary);
	return file.good();
}

void Join_Path(std::string *out, const char *dir, const char *name)
{
	if (dir == nullptr || name == nullptr) {
		return;
	}
	*out = dir;
	if (!out->empty() && out->back() != '/') {
		*out += '/';
	}
	*out += name;
}

#if defined(__linux__)
static bool Get_Executable_Directory(std::string *out_dir)
{
	char path[PATH_MAX];
	const ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
	if (len <= 0) {
		return false;
	}
	path[len] = '\0';
	char *slash = strrchr(path, '/');
	if (slash == nullptr) {
		return false;
	}
	*slash = '\0';
	*out_dir = path;
	return true;
}

static void Append_Resolved_Path(
	const std::string &dir,
	const char *filename,
	std::vector<std::string> *paths)
{
	char resolved[PATH_MAX];
	if (realpath(dir.c_str(), resolved) == nullptr) {
		return;
	}
	std::string candidate;
	Join_Path(&candidate, resolved, filename);
	paths->push_back(candidate);
}
#endif

static void Append_Search_Paths(const char *filename, std::vector<std::string> *paths)
{
	const char *env_path = getenv("RENEGADE_SHADER_PATH");
	if (env_path != nullptr && env_path[0] != '\0') {
		std::string candidate;
		Join_Path(&candidate, env_path, filename);
		paths->push_back(candidate);
	}

#if defined(__linux__)
	std::string exe_dir;
	if (Get_Executable_Directory(&exe_dir)) {
		Append_Resolved_Path(exe_dir + "/shaders", filename, paths);
		Append_Resolved_Path(exe_dir + "/../../shaders", filename, paths);
		Append_Resolved_Path(exe_dir, filename, paths);
	}
#endif

	{
		std::string candidate;
		Join_Path(&candidate, "shaders", filename);
		paths->push_back(candidate);
	}

#if defined(RENEGADE_VULKAN_SHADER_DIR)
	{
		std::string candidate;
		Join_Path(&candidate, RENEGADE_VULKAN_SHADER_DIR, filename);
		paths->push_back(candidate);
	}
#endif

	paths->push_back(filename);
}

} /* namespace */

VkShaderModule Load_Shader_Module(const std::vector<uint32_t> &spirv)
{
	if (spirv.empty() || spirv[0] != kSpirvMagic) {
		fprintf(stderr, "Invalid SPIR-V module (bad magic)\n");
		return VK_NULL_HANDLE;
	}

	VkShaderModuleCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = spirv.size() * sizeof(uint32_t);
	create_info.pCode = spirv.data();

	VkShaderModule module = VK_NULL_HANDLE;
	VkContext &ctx = VkContext::Get();
	VK_CHECK(vkCreateShaderModule(ctx.Device(), &create_info, nullptr, &module));
	return module;
}

void Destroy_Shader_Module(VkShaderModule module)
{
	if (module == VK_NULL_HANDLE) {
		return;
	}
	VkContext &ctx = VkContext::Get();
	vkDestroyShaderModule(ctx.Device(), module, nullptr);
}

bool Load_Spirv_From_File(const char *path, std::vector<uint32_t> *out_spirv)
{
	std::ifstream file(path, std::ios::ate | std::ios::binary);
	if (!file) {
		return false;
	}

	const size_t file_size = (size_t)file.tellg();
	if (file_size < sizeof(uint32_t) || file_size % sizeof(uint32_t) != 0) {
		fprintf(stderr, "Invalid SPIR-V file size: %s\n", path);
		return false;
	}

	out_spirv->resize(file_size / sizeof(uint32_t));
	file.seekg(0);
	file.read(reinterpret_cast<char *>(out_spirv->data()), (std::streamsize)file_size);
	if (!file.good()) {
		return false;
	}

	if ((*out_spirv)[0] != kSpirvMagic) {
		fprintf(stderr, "Invalid SPIR-V magic in: %s\n", path);
		out_spirv->clear();
		return false;
	}
	return true;
}

bool Load_Spirv_From_Search_Path(const char *filename, std::vector<uint32_t> *out_spirv)
{
	std::vector<std::string> paths;
	Append_Search_Paths(filename, &paths);

	for (size_t i = 0; i < paths.size(); ++i) {
		if (Load_Spirv_From_File(paths[i].c_str(), out_spirv)) {
			fprintf(stderr, "Loaded SPIR-V: %s\n", paths[i].c_str());
			return true;
		}
	}

	fprintf(stderr, "Failed to load SPIR-V '%s'. Searched:\n", filename);
	for (size_t i = 0; i < paths.size(); ++i) {
		fprintf(stderr, "  %s\n", paths[i].c_str());
	}
	return false;
}

} /* namespace ww3d_vulkan */
