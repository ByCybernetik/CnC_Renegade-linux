#ifndef WW3D2_VULKAN_VK_FORMAT_H
#define WW3D2_VULKAN_VK_FORMAT_H

#include <vulkan/vulkan.h>
#include "../ww3d2/ww3dformat.h"

namespace ww3d_vulkan {

VkFormat WW3D_Format_To_Vk(WW3DFormat format);
WW3DFormat Vk_Format_To_WW3D(VkFormat format);
uint32_t Vk_Format_Bytes_Per_Pixel(VkFormat format);

} /* namespace ww3d_vulkan */

#endif
