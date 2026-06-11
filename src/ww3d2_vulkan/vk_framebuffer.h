#ifndef WW3D2_VULKAN_VK_FRAMEBUFFER_H
#define WW3D2_VULKAN_VK_FRAMEBUFFER_H

#include "vk_platform.h"
#include <vector>

namespace ww3d_vulkan {

class VkFramebufferMgr {
public:
	bool Create(
		VkRenderPass render_pass,
		const std::vector<VkImageView> &swapchain_views,
		VkFormat depth_format,
		VkExtent2D extent);
	void Destroy();

	VkFramebuffer Framebuffer(uint32_t index) const { return framebuffers_[index]; }
	VkImage Depth_Image(uint32_t index) const { return depth_images_[index]; }
	VkImageView Depth_View(uint32_t index) const { return depth_views_[index]; }

private:
	bool Create_Depth_Resources(
		VkFormat depth_format,
		VkExtent2D extent,
		uint32_t count);

	std::vector<VkFramebuffer> framebuffers_;
	std::vector<VkImage> depth_images_;
	std::vector<VkDeviceMemory> depth_memory_;
	std::vector<VkImageView> depth_views_;
};

} /* namespace ww3d_vulkan */

#endif
