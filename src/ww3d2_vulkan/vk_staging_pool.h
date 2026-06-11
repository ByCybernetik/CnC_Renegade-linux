#ifndef WW3D2_VULKAN_VK_STAGING_POOL_H
#define WW3D2_VULKAN_VK_STAGING_POOL_H

#include <vulkan/vulkan.h>
#include "vk_allocator.h"
#include <vector>

namespace ww3d_vulkan {

class VkStagingPool {
public:
	struct Entry {
		VkBuffer buffer = VK_NULL_HANDLE;
		VmaAllocation alloc = VK_NULL_HANDLE;
		VkDeviceSize size = 0;
		void *mapped = nullptr;
	};

	static Entry Acquire(VkDeviceSize size);
	static void Release(Entry entry);
	static void Reset();

private:
	struct PooledBuffer {
		VkBuffer buffer;
		VmaAllocation alloc;
		VkDeviceSize size;
		void *mapped;
		bool in_use;
	};

	static std::vector<PooledBuffer> &Pool();
};

} /* namespace ww3d_vulkan */

#endif
