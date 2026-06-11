#include "vk_framebuffer.h"
#include "vk_check.h"
#include "vk_context.h"

namespace ww3d_vulkan {

namespace {

uint32_t Find_Memory_Type(uint32_t type_bits, VkMemoryPropertyFlags properties)
{
	VkContext &ctx = VkContext::Get();
	VkPhysicalDeviceMemoryProperties mem_props = {};
	vkGetPhysicalDeviceMemoryProperties(ctx.Physical_Device(), &mem_props);
	for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
		if ((type_bits & (1u << i)) &&
			(mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	std::abort();
	return 0;
}

} /* namespace */

bool VkFramebufferMgr::Create(
	VkRenderPass render_pass,
	const std::vector<VkImageView> &swapchain_views,
	VkFormat depth_format,
	VkExtent2D extent)
{
	Destroy();

	if (!Create_Depth_Resources(depth_format, extent, (uint32_t)swapchain_views.size())) {
		return false;
	}

	VkContext &ctx = VkContext::Get();
	framebuffers_.resize(swapchain_views.size());

	for (size_t i = 0; i < swapchain_views.size(); ++i) {
		VkImageView attachments[] = {swapchain_views[i], depth_views_[i]};

		VkFramebufferCreateInfo framebuffer_info = {};
		framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_info.renderPass = render_pass;
		framebuffer_info.attachmentCount = 2;
		framebuffer_info.pAttachments = attachments;
		framebuffer_info.width = extent.width;
		framebuffer_info.height = extent.height;
		framebuffer_info.layers = 1;
		VK_CHECK(vkCreateFramebuffer(
			ctx.Device(), &framebuffer_info, nullptr, &framebuffers_[i]));
	}
	return true;
}

void VkFramebufferMgr::Destroy()
{
	VkContext &ctx = VkContext::Get();
	if (ctx.Device() != VK_NULL_HANDLE) {
		for (size_t i = 0; i < framebuffers_.size(); ++i) {
			vkDestroyFramebuffer(ctx.Device(), framebuffers_[i], nullptr);
		}
		for (size_t i = 0; i < depth_views_.size(); ++i) {
			vkDestroyImageView(ctx.Device(), depth_views_[i], nullptr);
		}
		for (size_t i = 0; i < depth_images_.size(); ++i) {
			vkDestroyImage(ctx.Device(), depth_images_[i], nullptr);
		}
		for (size_t i = 0; i < depth_memory_.size(); ++i) {
			vkFreeMemory(ctx.Device(), depth_memory_[i], nullptr);
		}
	}
	framebuffers_.clear();
	depth_views_.clear();
	depth_images_.clear();
	depth_memory_.clear();
}

bool VkFramebufferMgr::Create_Depth_Resources(
	VkFormat depth_format,
	VkExtent2D extent,
	uint32_t count)
{
	VkContext &ctx = VkContext::Get();
	depth_images_.resize(count);
	depth_memory_.resize(count);
	depth_views_.resize(count);

	for (uint32_t i = 0; i < count; ++i) {
		VkImageCreateInfo image_info = {};
		image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_info.imageType = VK_IMAGE_TYPE_2D;
		image_info.extent.width = extent.width;
		image_info.extent.height = extent.height;
		image_info.extent.depth = 1;
		image_info.mipLevels = 1;
		image_info.arrayLayers = 1;
		image_info.format = depth_format;
		image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		image_info.samples = VK_SAMPLE_COUNT_1_BIT;
		VK_CHECK(vkCreateImage(ctx.Device(), &image_info, nullptr, &depth_images_[i]));

		VkMemoryRequirements requirements = {};
		vkGetImageMemoryRequirements(ctx.Device(), depth_images_[i], &requirements);

		VkMemoryAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = requirements.size;
		alloc_info.memoryTypeIndex = Find_Memory_Type(
			requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK(vkAllocateMemory(ctx.Device(), &alloc_info, nullptr, &depth_memory_[i]));
		VK_CHECK(vkBindImageMemory(ctx.Device(), depth_images_[i], depth_memory_[i], 0));

		VkImageViewCreateInfo view_info = {};
		view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_info.image = depth_images_[i];
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_info.format = depth_format;
		view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		view_info.subresourceRange.baseMipLevel = 0;
		view_info.subresourceRange.levelCount = 1;
		view_info.subresourceRange.baseArrayLayer = 0;
		view_info.subresourceRange.layerCount = 1;
		VK_CHECK(vkCreateImageView(ctx.Device(), &view_info, nullptr, &depth_views_[i]));
	}
	return true;
}

} /* namespace ww3d_vulkan */
