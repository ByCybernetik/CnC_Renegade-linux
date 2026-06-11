#ifndef WW3D2_VULKAN_VK_BUFFER_H
#define WW3D2_VULKAN_VK_BUFFER_H

#include <vulkan/vulkan.h>
#include "vk_allocator.h"
#include <cstdint>

namespace ww3d_vulkan {

class VkBufferAlloc {
public:
	VkBufferAlloc() = default;
	~VkBufferAlloc();

	VkBufferAlloc(const VkBufferAlloc &) = delete;
	VkBufferAlloc &operator=(const VkBufferAlloc &) = delete;

	bool Create(
		VkDeviceSize size,
		VkBufferUsageFlags usage,
		VmaMemoryUsage vma_usage);
	void Destroy();

	void Upload(const void *data, VkDeviceSize size, VkDeviceSize offset = 0);
	void *Map();
	void Unmap();

	VkBuffer Handle() const { return buffer_; }
	VkDeviceSize Size() const { return size_; }

private:
	VkBuffer buffer_ = VK_NULL_HANDLE;
	VmaAllocation allocation_ = VK_NULL_HANDLE;
	VkDeviceSize size_ = 0;
	void *mapped_ = nullptr;
};

} /* namespace ww3d_vulkan */

#endif
