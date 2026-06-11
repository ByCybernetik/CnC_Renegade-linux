#include "vk_gpu_buffer.h"

namespace ww3d_vulkan {

GpuBuffer::~GpuBuffer()
{
	Destroy();
}

bool GpuBuffer::Create(VkDeviceSize size, VkBufferUsageFlags usage)
{
	Destroy();
	return alloc_.Create(size, usage, VMA_MEMORY_USAGE_CPU_TO_GPU);
}

void GpuBuffer::Destroy()
{
	Unlock();
	alloc_.Destroy();
}

unsigned char *GpuBuffer::Lock(VkDeviceSize offset, VkDeviceSize size)
{
	(void)size;
	if (lock_base_ != nullptr) {
		return lock_base_ + offset;
	}
	lock_base_ = static_cast<unsigned char *>(alloc_.Map());
	if (lock_base_ == nullptr) {
		return nullptr;
	}
	return lock_base_ + offset;
}

void GpuBuffer::Unlock()
{
	if (lock_base_ != nullptr) {
		alloc_.Unmap();
		lock_base_ = nullptr;
	}
}

} /* namespace ww3d_vulkan */
