#ifndef WW3D2_VULKAN_VK_GPU_BUFFER_H
#define WW3D2_VULKAN_VK_GPU_BUFFER_H

#include "vk_buffer.h"
#include <vulkan/vulkan.h>

namespace ww3d_vulkan {

class GpuBuffer {
public:
	GpuBuffer() = default;
	~GpuBuffer();

	GpuBuffer(const GpuBuffer &) = delete;
	GpuBuffer &operator=(const GpuBuffer &) = delete;

	bool Create(VkDeviceSize size, VkBufferUsageFlags usage);
	void Destroy();

	unsigned char *Lock(VkDeviceSize offset, VkDeviceSize size);
	void Unlock();

	VkBuffer Handle() const { return alloc_.Handle(); }
	VkDeviceSize Size() const { return alloc_.Size(); }

private:
	VkBufferAlloc alloc_;
	unsigned char *lock_base_ = nullptr;
};

} /* namespace ww3d_vulkan */

#endif
