#ifndef WW3D2_VULKAN_VK_DESCRIPTOR_H
#define WW3D2_VULKAN_VK_DESCRIPTOR_H

#include <vulkan/vulkan.h>

namespace ww3d_vulkan {

class VkDescriptorMgr {
public:
	bool Create(VkDescriptorSetLayout layout, uint32_t set_count);
	void Destroy();

	VkDescriptorSet Set(uint32_t index) const;

private:
	VkDescriptorPool pool_ = VK_NULL_HANDLE;
	VkDescriptorSet *sets_ = nullptr;
	uint32_t set_count_ = 0;
};

} /* namespace ww3d_vulkan */

#endif
