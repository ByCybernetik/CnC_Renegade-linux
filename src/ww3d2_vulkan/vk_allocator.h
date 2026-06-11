#ifndef WW3D2_VULKAN_VK_ALLOCATOR_H
#define WW3D2_VULKAN_VK_ALLOCATOR_H

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace ww3d_vulkan {

class VkAllocator {
public:
	static bool Init(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device, uint32_t api_version);
	static void Shutdown();
	static VmaAllocator Handle();

	static bool Create_Buffer(
		VkDeviceSize size,
		VkBufferUsageFlags usage,
		VmaMemoryUsage vma_usage,
		VkBuffer *out_buffer,
		VmaAllocation *out_alloc);
	static void Destroy_Buffer(VkBuffer buffer, VmaAllocation alloc);

	static bool Create_Image(
		const VkImageCreateInfo *create_info,
		VmaMemoryUsage vma_usage,
		VkImage *out_image,
		VmaAllocation *out_alloc);
	static void Destroy_Image(VkImage image, VmaAllocation alloc);

	static void *Map(VmaAllocation alloc);
	static void Unmap(VmaAllocation alloc);
};

} /* namespace ww3d_vulkan */

#endif
