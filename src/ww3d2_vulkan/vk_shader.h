#ifndef WW3D2_VULKAN_VK_SHADER_H
#define WW3D2_VULKAN_VK_SHADER_H

#include <vulkan/vulkan.h>
#include <vector>

namespace ww3d_vulkan {

VkShaderModule Load_Shader_Module(const std::vector<uint32_t> &spirv);
void Destroy_Shader_Module(VkShaderModule module);

bool Load_Spirv_From_File(const char *path, std::vector<uint32_t> *out_spirv);
bool Load_Spirv_From_Search_Path(const char *filename, std::vector<uint32_t> *out_spirv);

} /* namespace ww3d_vulkan */

#endif
