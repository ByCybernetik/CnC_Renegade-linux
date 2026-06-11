#include "vk_swapchain.h"
#include "vk_check.h"
#include "vk_context.h"

#include <algorithm>
#include <limits>

namespace ww3d_vulkan {

namespace {

VkSurfaceFormatKHR Choose_Surface_Format(const std::vector<VkSurfaceFormatKHR> &formats)
{
	for (size_t i = 0; i < formats.size(); ++i) {
		if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
			formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return formats[i];
		}
	}
	return formats.empty() ? VkSurfaceFormatKHR{} : formats[0];
}

VkPresentModeKHR Choose_Present_Mode(
	const std::vector<VkPresentModeKHR> &modes,
	bool vsync)
{
	if (!vsync) {
		for (size_t i = 0; i < modes.size(); ++i) {
			if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
				return VK_PRESENT_MODE_MAILBOX_KHR;
			}
		}
		for (size_t i = 0; i < modes.size(); ++i) {
			if (modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
				return VK_PRESENT_MODE_IMMEDIATE_KHR;
			}
		}
	}
	return VK_PRESENT_MODE_FIFO_KHR;
}

} /* namespace */

bool VkSwapchain::Create(uint32_t width, uint32_t height, bool vsync)
{
	Destroy();
	return Create_Swapchain(width, height, vsync) && Create_Image_Views();
}

void VkSwapchain::Destroy()
{
	VkContext &ctx = VkContext::Get();
	if (ctx.Device() == VK_NULL_HANDLE) {
		return;
	}

	for (size_t i = 0; i < image_views_.size(); ++i) {
		vkDestroyImageView(ctx.Device(), image_views_[i], nullptr);
	}
	image_views_.clear();
	images_.clear();

	if (swapchain_ != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(ctx.Device(), swapchain_, nullptr);
		swapchain_ = VK_NULL_HANDLE;
	}
}

void VkSwapchain::Recreate(uint32_t width, uint32_t height, bool vsync)
{
	VkContext &ctx = VkContext::Get();
	vkDeviceWaitIdle(ctx.Device());
	Create(width, height, vsync);
}

bool VkSwapchain::Create_Swapchain(uint32_t width, uint32_t height, bool vsync)
{
	VkContext &ctx = VkContext::Get();

	VkSurfaceCapabilitiesKHR capabilities = {};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		ctx.Physical_Device(), ctx.Surface(), &capabilities);

	uint32_t format_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(
		ctx.Physical_Device(), ctx.Surface(), &format_count, nullptr);
	std::vector<VkSurfaceFormatKHR> formats(format_count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(
		ctx.Physical_Device(), ctx.Surface(), &format_count, formats.data());

	uint32_t present_mode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(
		ctx.Physical_Device(), ctx.Surface(), &present_mode_count, nullptr);
	std::vector<VkPresentModeKHR> present_modes(present_mode_count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(
		ctx.Physical_Device(), ctx.Surface(), &present_mode_count, present_modes.data());

	VkSurfaceFormatKHR surface_format = Choose_Surface_Format(formats);
	VkPresentModeKHR present_mode = Choose_Present_Mode(present_modes, vsync);

	VkExtent2D extent = capabilities.currentExtent;
	if (extent.width == UINT32_MAX) {
		extent.width = std::max(width, 1u);
		extent.height = std::max(height, 1u);
	}

	uint32_t image_count = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
		image_count = capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = ctx.Surface();
	create_info.minImageCount = image_count;
	create_info.imageFormat = surface_format.format;
	create_info.imageColorSpace = surface_format.colorSpace;
	create_info.imageExtent = extent;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_info.preTransform = capabilities.currentTransform;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode = present_mode;
	create_info.clipped = VK_TRUE;
	create_info.oldSwapchain = VK_NULL_HANDLE;

	uint32_t queue_indices[] = {
		ctx.Graphics_Queue_Family(),
		ctx.Present_Queue_Family(),
	};
	if (ctx.Graphics_Queue_Family() != ctx.Present_Queue_Family()) {
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = queue_indices;
	}

	VK_CHECK(vkCreateSwapchainKHR(ctx.Device(), &create_info, nullptr, &swapchain_));

	vkGetSwapchainImagesKHR(ctx.Device(), swapchain_, &image_count, nullptr);
	images_.resize(image_count);
	vkGetSwapchainImagesKHR(ctx.Device(), swapchain_, &image_count, images_.data());

	image_format_ = surface_format.format;
	extent_ = extent;
	return true;
}

bool VkSwapchain::Create_Image_Views()
{
	VkContext &ctx = VkContext::Get();
	image_views_.resize(images_.size());

	for (size_t i = 0; i < images_.size(); ++i) {
		VkImageViewCreateInfo view_info = {};
		view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_info.image = images_[i];
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_info.format = image_format_;
		view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_info.subresourceRange.baseMipLevel = 0;
		view_info.subresourceRange.levelCount = 1;
		view_info.subresourceRange.baseArrayLayer = 0;
		view_info.subresourceRange.layerCount = 1;
		VK_CHECK(vkCreateImageView(ctx.Device(), &view_info, nullptr, &image_views_[i]));
	}
	return true;
}

bool VkSwapchain::Acquire_Next_Image(
	uint32_t frame_index,
	VkSemaphore image_available,
	uint32_t *image_index)
{
	(void)frame_index;
	VkContext &ctx = VkContext::Get();
	VkResult result = vkAcquireNextImageKHR(
		ctx.Device(),
		swapchain_,
		UINT64_MAX,
		image_available,
		VK_NULL_HANDLE,
		image_index);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		return false;
	}
	if (result == VK_SUBOPTIMAL_KHR) {
		// Swapchain is still usable; image_index is valid.
	}
	VK_CHECK(result);
	return true;
}

bool VkSwapchain::Present(
	uint32_t frame_index,
	VkSemaphore render_finished,
	uint32_t image_index)
{
	(void)frame_index;
	VkContext &ctx = VkContext::Get();

	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &render_finished;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &swapchain_;
	present_info.pImageIndices = &image_index;

	VkResult result = vkQueuePresentKHR(ctx.Present_Queue(), &present_info);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		return false;
	}
	if (result == VK_SUBOPTIMAL_KHR) {
		return true;
	}
	VK_CHECK(result);
	return true;
}

} /* namespace ww3d_vulkan */
