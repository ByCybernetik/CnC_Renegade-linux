#include "vk_descriptor.h"
#include "vk_check.h"
#include "vk_context.h"

namespace ww3d_vulkan {

bool VkDescriptorMgr::Create(VkDescriptorSetLayout layout, uint32_t set_count)
{
	Destroy();
	set_count_ = set_count;

	VkDescriptorPoolSize pool_sizes[2] = {};
	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_sizes[0].descriptorCount = set_count;
	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes[1].descriptorCount = set_count * 2;

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.poolSizeCount = 2;
	pool_info.pPoolSizes = pool_sizes;
	pool_info.maxSets = set_count;

	VkContext &ctx = VkContext::Get();
	VK_CHECK(vkCreateDescriptorPool(ctx.Device(), &pool_info, nullptr, &pool_));

	sets_ = new VkDescriptorSet[set_count];
	VkDescriptorSetLayout *layouts = new VkDescriptorSetLayout[set_count];
	for (uint32_t i = 0; i < set_count; ++i) {
		layouts[i] = layout;
	}

	VkDescriptorSetAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = pool_;
	alloc_info.descriptorSetCount = set_count;
	alloc_info.pSetLayouts = layouts;
	VK_CHECK(vkAllocateDescriptorSets(ctx.Device(), &alloc_info, sets_));
	delete[] layouts;
	return true;
}

void VkDescriptorMgr::Destroy()
{
	VkContext &ctx = VkContext::Get();
	delete[] sets_;
	sets_ = nullptr;
	set_count_ = 0;
	if (pool_ != VK_NULL_HANDLE && ctx.Device() != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(ctx.Device(), pool_, nullptr);
	}
	pool_ = VK_NULL_HANDLE;
}

VkDescriptorSet VkDescriptorMgr::Set(uint32_t index) const
{
	return sets_[index];
}

} /* namespace ww3d_vulkan */
