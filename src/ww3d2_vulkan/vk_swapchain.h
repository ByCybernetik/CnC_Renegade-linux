#ifndef WW3D2_VULKAN_VK_SWAPCHAIN_H
#define WW3D2_VULKAN_VK_SWAPCHAIN_H

#include "vk_platform.h"
#include <vector>

namespace ww3d_vulkan {

class VkSwapchain {
public:
	bool Create(uint32_t width, uint32_t height, bool vsync);
	void Destroy();
	void Recreate(uint32_t width, uint32_t height, bool vsync);

	bool Acquire_Next_Image(uint32_t frame_index, VkSemaphore image_available, uint32_t *image_index);
	bool Present(uint32_t frame_index, VkSemaphore render_finished, uint32_t image_index);

	VkSwapchainKHR Handle() const { return swapchain_; }
	VkFormat Image_Format() const { return image_format_; }
	VkExtent2D Extent() const { return extent_; }
	uint32_t Image_Count() const { return (uint32_t)images_.size(); }
	VkImage Image(uint32_t index) const { return images_[index]; }
	VkImageView Image_View(uint32_t index) const { return image_views_[index]; }

private:
	bool Create_Swapchain(uint32_t width, uint32_t height, bool vsync);
	bool Create_Image_Views();

	VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
	VkFormat image_format_ = VK_FORMAT_UNDEFINED;
	VkExtent2D extent_ = {};
	std::vector<VkImage> images_;
	std::vector<VkImageView> image_views_;
};

} /* namespace ww3d_vulkan */

#endif
