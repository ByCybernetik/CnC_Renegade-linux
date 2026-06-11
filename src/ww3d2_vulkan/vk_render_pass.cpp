#include "vk_render_pass.h"
#include "vk_check.h"
#include "vk_context.h"

namespace ww3d_vulkan {

bool VkRenderPassMgr::Create_Internal(
	VkFormat color_format,
	VkImageLayout final_color_layout,
	VkRenderPass *out)
{
	if (out == nullptr) {
		return false;
	}

	if (depth_format_ == VK_FORMAT_UNDEFINED) {
		depth_format_ = VK_FORMAT_D24_UNORM_S8_UINT;
		VkContext &ctx = VkContext::Get();
		VkFormat candidates[] = {
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D16_UNORM,
		};
		for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
			VkFormatProperties props = {};
			vkGetPhysicalDeviceFormatProperties(
				ctx.Physical_Device(), candidates[i], &props);
			if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
				depth_format_ = candidates[i];
				break;
			}
		}
	}

	VkAttachmentDescription attachments[2] = {};
	attachments[0].format = color_format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = final_color_layout;

	attachments[1].format = depth_format_;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference color_ref = {};
	color_ref.attachment = 0;
	color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_ref = {};
	depth_ref.attachment = 1;
	depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_ref;
	subpass.pDepthStencilAttachment = &depth_ref;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.attachmentCount = 2;
	render_pass_info.pAttachments = attachments;
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;
	render_pass_info.dependencyCount = 1;
	render_pass_info.pDependencies = &dependency;

	VkContext &ctx = VkContext::Get();
	VK_CHECK(vkCreateRenderPass(ctx.Device(), &render_pass_info, nullptr, out));
	return true;
}

bool VkRenderPassMgr::Create(VkFormat color_format)
{
	Destroy();
	return Create_Internal(color_format, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, &render_pass_);
}

bool VkRenderPassMgr::Create_Offscreen(VkFormat color_format)
{
	if (offscreen_render_pass_ != VK_NULL_HANDLE) {
		VkContext &ctx = VkContext::Get();
		vkDestroyRenderPass(ctx.Device(), offscreen_render_pass_, nullptr);
		offscreen_render_pass_ = VK_NULL_HANDLE;
	}
	return Create_Internal(
		color_format,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		&offscreen_render_pass_);
}

void VkRenderPassMgr::Destroy()
{
	VkContext &ctx = VkContext::Get();
	if (ctx.Device() != VK_NULL_HANDLE) {
		if (render_pass_ != VK_NULL_HANDLE) {
			vkDestroyRenderPass(ctx.Device(), render_pass_, nullptr);
		}
		if (offscreen_render_pass_ != VK_NULL_HANDLE) {
			vkDestroyRenderPass(ctx.Device(), offscreen_render_pass_, nullptr);
		}
	}
	render_pass_ = VK_NULL_HANDLE;
	offscreen_render_pass_ = VK_NULL_HANDLE;
	depth_format_ = VK_FORMAT_UNDEFINED;
}

} /* namespace ww3d_vulkan */
