#include "vk_staging_pool.h"
#include "vk_check.h"
#include "vk_context.h"

#include <algorithm>
#include <cstring>

namespace ww3d_vulkan {

std::vector<VkStagingPool::PooledBuffer> &VkStagingPool::Pool()
{
	static std::vector<PooledBuffer> pool;
	return pool;
}

VkStagingPool::Entry VkStagingPool::Acquire(VkDeviceSize size)
{
	auto &pool = Pool();

	/* Try to find a free buffer that is large enough. */
	for (auto &pb : pool) {
		if (!pb.in_use && pb.size >= size) {
			pb.in_use = true;
			Entry e;
			e.buffer = pb.buffer;
			e.alloc = pb.alloc;
			e.size = pb.size;
			e.mapped = pb.mapped;
			return e;
		}
	}

	/* No suitable free buffer — create a new one via VMA. */
	PooledBuffer pb = {};
	pb.size = size;
	pb.in_use = true;

	if (!VkAllocator::Create_Buffer(
			size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_CPU_ONLY,
			&pb.buffer,
			&pb.alloc)) {
		fprintf(stderr, "VkStagingPool: failed to allocate staging buffer of size %zu\n", (size_t)size);
		return Entry{};
	}

	pb.mapped = VkAllocator::Map(pb.alloc);

	pool.push_back(pb);

	Entry e;
	e.buffer = pb.buffer;
	e.alloc = pb.alloc;
	e.size = pb.size;
	e.mapped = pb.mapped;
	return e;
}

void VkStagingPool::Release(Entry entry)
{
	if (entry.buffer == VK_NULL_HANDLE) {
		return;
	}

	auto &pool = Pool();
	for (auto &pb : pool) {
		if (pb.buffer == entry.buffer) {
			pb.in_use = false;
			return;
		}
	}

	/* If not found in pool (shouldn't happen), destroy it. */
	VkAllocator::Destroy_Buffer(entry.buffer, entry.alloc);
}

void VkStagingPool::Reset()
{
	auto &pool = Pool();
	for (auto &pb : pool) {
		if (pb.mapped != nullptr) {
			VkAllocator::Unmap(pb.alloc);
		}
		VkAllocator::Destroy_Buffer(pb.buffer, pb.alloc);
	}
	pool.clear();
}

} /* namespace ww3d_vulkan */
