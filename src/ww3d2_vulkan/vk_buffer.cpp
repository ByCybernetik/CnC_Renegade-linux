#include "vk_buffer.h"
#include "vk_check.h"
#include "vk_context.h"

#include <cstring>

namespace ww3d_vulkan {

VkBufferAlloc::~VkBufferAlloc()
{
	Destroy();
}

bool VkBufferAlloc::Create(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage vma_usage)
{
	Destroy();

	size_ = size;
	if (!VkAllocator::Create_Buffer(size, usage, vma_usage, &buffer_, &allocation_)) {
		buffer_ = VK_NULL_HANDLE;
		allocation_ = VK_NULL_HANDLE;
		size_ = 0;
		return false;
	}
	return true;
}

void VkBufferAlloc::Destroy()
{
	if (buffer_ != VK_NULL_HANDLE) {
		if (mapped_ != nullptr) {
			Unmap();
		}
		VkAllocator::Destroy_Buffer(buffer_, allocation_);
		buffer_ = VK_NULL_HANDLE;
		allocation_ = VK_NULL_HANDLE;
		size_ = 0;
	}
}

void VkBufferAlloc::Upload(const void *data, VkDeviceSize size, VkDeviceSize offset)
{
	void *ptr = Map();
	memcpy(static_cast<char *>(ptr) + offset, data, (size_t)size);
	Unmap();
}

void *VkBufferAlloc::Map()
{
	if (mapped_ != nullptr) {
		return mapped_;
	}
	mapped_ = VkAllocator::Map(allocation_);
	return mapped_;
}

void VkBufferAlloc::Unmap()
{
	if (mapped_ == nullptr) {
		return;
	}
	VkAllocator::Unmap(allocation_);
	mapped_ = nullptr;
}

} /* namespace ww3d_vulkan */
