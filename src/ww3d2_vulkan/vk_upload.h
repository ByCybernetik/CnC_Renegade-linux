#ifndef WW3D2_VULKAN_VK_UPLOAD_H
#define WW3D2_VULKAN_VK_UPLOAD_H

#include <vulkan/vulkan.h>

namespace ww3d_vulkan {

void Submit_One_Time_Commands(void (*record)(VkCommandBuffer cmd, void *user), void *user);

} /* namespace ww3d_vulkan */

#endif
