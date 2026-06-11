#ifndef WW3D2_VULKAN_VK_RENDER_PASS_H
#define WW3D2_VULKAN_VK_RENDER_PASS_H

#include <vulkan/vulkan.h>

namespace ww3d_vulkan {

class VkRenderPassMgr {
public:
	bool Create(VkFormat color_format);
	bool Create_Offscreen(VkFormat color_format);
	void Destroy();

	VkRenderPass Handle() const { return render_pass_; }
	VkRenderPass Offscreen_Handle() const { return offscreen_render_pass_; }
	VkFormat Depth_Format() const { return depth_format_; }

private:
	bool Create_Internal(VkFormat color_format, VkImageLayout final_color_layout, VkRenderPass *out);

	VkRenderPass render_pass_ = VK_NULL_HANDLE;
	VkRenderPass offscreen_render_pass_ = VK_NULL_HANDLE;
	VkFormat depth_format_ = VK_FORMAT_UNDEFINED;
};

} /* namespace ww3d_vulkan */

#endif
