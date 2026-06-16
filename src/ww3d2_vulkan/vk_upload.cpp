#include "vk_upload.h"
#include "vk_check.h"
#include "vk_context.h"

namespace ww3d_vulkan {

void Submit_One_Time_Commands(void (*record)(VkCommandBuffer cmd, void *user), void *user)
{
	if (record == nullptr) {
		return;
	}

	VkContext &ctx = VkContext::Get();
	VkCommandBuffer cmd = VK_NULL_HANDLE;

	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = ctx.Command_Pool();
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;
	VK_CHECK(vkAllocateCommandBuffers(ctx.Device(), &alloc_info, &cmd));

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));

	record(cmd, user);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd;

	VkFence fence = VK_NULL_HANDLE;
	VkFenceCreateInfo fence_info = {};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VK_CHECK(vkCreateFence(ctx.Device(), &fence_info, nullptr, &fence));

	VK_CHECK(vkQueueSubmit(ctx.Graphics_Queue(), 1, &submit_info, fence));
	VK_CHECK(vkWaitForFences(ctx.Device(), 1, &fence, VK_TRUE, UINT64_MAX));

	vkDestroyFence(ctx.Device(), fence, nullptr);
	vkFreeCommandBuffers(ctx.Device(), ctx.Command_Pool(), 1, &cmd);
}

} /* namespace ww3d_vulkan */
