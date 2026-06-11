#ifndef WW3D2_VULKAN_VK_TEXTURE_H
#define WW3D2_VULKAN_VK_TEXTURE_H

#include <vulkan/vulkan.h>
#include "vk_allocator.h"
#include "../ww3d2/ww3dformat.h"

class DDSFileClass;

namespace ww3d_vulkan {

class VkTexture {
public:
	bool Create_Empty(uint32_t width, uint32_t height, WW3DFormat format, bool clamp_uv = false);
	bool Create_Solid(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
	bool Create_From_DDS(
		const DDSFileClass &dds,
		VkSamplerAddressMode address_u,
		VkSamplerAddressMode address_v);
	bool Create_From_Compressed(
		WW3DFormat format,
		uint32_t width,
		uint32_t height,
		uint32_t mip_levels,
		const uint8_t *compressed_data,
		size_t compressed_size,
		VkSamplerAddressMode address_u,
		VkSamplerAddressMode address_v);
	bool Create_From_Rgba8(
		const uint8_t *rgba,
		uint32_t width,
		uint32_t height,
		VkSamplerAddressMode address_u,
		VkSamplerAddressMode address_v);
	bool Create_As_Render_Target(
		uint32_t width,
		uint32_t height,
		WW3DFormat format,
		VkRenderPass render_pass,
		VkFormat depth_format);
	bool Upload_Rgb565_Region(
		const unsigned char *src,
		int src_pitch,
		uint32_t dst_x,
		uint32_t dst_y,
		uint32_t copy_w,
		uint32_t copy_h);
	bool Upload_Rgba8_Region(
		const unsigned char *src,
		int src_pitch,
		uint32_t dst_x,
		uint32_t dst_y,
		uint32_t copy_w,
		uint32_t copy_h);
	void Destroy();

	VkImage Image() const { return image_; }
	VkImageView View() const { return view_; }
	VkSampler Sampler() const { return sampler_; }
	VkImageLayout Layout() const { return layout_; }
	bool Is_Render_Target() const { return is_render_target_; }
	VkFramebuffer Render_Framebuffer() const { return framebuffer_; }
	VkExtent2D Render_Extent() const { return render_extent_; }

	uint32_t Width() const { return width_; }
	uint32_t Height() const { return height_; }
	uint32_t Mip_Levels() const { return mip_levels_; }

	void Set_Layout_Shader_Read_Only() { layout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; }

private:
	VkImage image_ = VK_NULL_HANDLE;
	VmaAllocation allocation_ = VK_NULL_HANDLE;
	VkImageView view_ = VK_NULL_HANDLE;
	VkSampler sampler_ = VK_NULL_HANDLE;
	VkImageLayout layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
	uint32_t width_ = 0;
	uint32_t height_ = 0;
	uint32_t mip_levels_ = 1;
	bool is_render_target_ = false;
	VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
	VkImage depth_image_ = VK_NULL_HANDLE;
	VmaAllocation depth_allocation_ = VK_NULL_HANDLE;
	VkImageView depth_view_ = VK_NULL_HANDLE;
	VkExtent2D render_extent_ = {};
};

} /* namespace ww3d_vulkan */

#endif
