/* VMA's built-in vma_aligned_alloc() returns NULL on Linux when compiled as
 * C++11 with libstdc++ that defines _GLIBCXX_HAVE_ALIGNED_ALLOC (Arch, Fedora,
 * etc.).  vma_new() then placement-news VmaAllocator_T onto nullptr → SIGSEGV. */
#if defined(__linux__) && !defined(VMA_SYSTEM_ALIGNED_MALLOC)
#include <cstdlib>
static void *Renegade_Vma_Aligned_Alloc(size_t alignment, size_t size)
{
	if (alignment < sizeof(void *)) {
		alignment = sizeof(void *);
	}
	void *ptr = nullptr;
	if (posix_memalign(&ptr, alignment, size) != 0) {
		return nullptr;
	}
	return ptr;
}
#define VMA_SYSTEM_ALIGNED_MALLOC(size, alignment) Renegade_Vma_Aligned_Alloc((alignment), (size))
#define VMA_SYSTEM_ALIGNED_FREE(ptr)             free(ptr)
#endif

#define VMA_IMPLEMENTATION
#include "vk_allocator.h"
#include "vk_check.h"

namespace ww3d_vulkan {

static VmaAllocator g_allocator = VK_NULL_HANDLE;

bool VkAllocator::Init(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device, uint32_t api_version)
{
	if (g_allocator != VK_NULL_HANDLE) {
		return true;
	}

	VmaAllocatorCreateInfo allocator_info = {};
	allocator_info.vulkanApiVersion = api_version;
	allocator_info.physicalDevice = physical_device;
	allocator_info.device = device;
	allocator_info.instance = instance;

	VkResult result = vmaCreateAllocator(&allocator_info, &g_allocator);
	return result == VK_SUCCESS;
}

void VkAllocator::Shutdown()
{
	if (g_allocator != VK_NULL_HANDLE) {
		vmaDestroyAllocator(g_allocator);
		g_allocator = VK_NULL_HANDLE;
	}
}

VmaAllocator VkAllocator::Handle()
{
	return g_allocator;
}

bool VkAllocator::Create_Buffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage vma_usage, VkBuffer *out_buffer, VmaAllocation *out_alloc)
{
	VkBufferCreateInfo buffer_info = {};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.size = size;
	buffer_info.usage = usage;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo alloc_info = {};
	alloc_info.usage = vma_usage;

	VkResult result = vmaCreateBuffer(g_allocator, &buffer_info, &alloc_info, out_buffer, out_alloc, nullptr);
	return result == VK_SUCCESS;
}

void VkAllocator::Destroy_Buffer(VkBuffer buffer, VmaAllocation alloc)
{
	if (g_allocator == VK_NULL_HANDLE) {
		return;
	}
	vmaDestroyBuffer(g_allocator, buffer, alloc);
}

bool VkAllocator::Create_Image(const VkImageCreateInfo *create_info, VmaMemoryUsage vma_usage, VkImage *out_image, VmaAllocation *out_alloc)
{
	VmaAllocationCreateInfo alloc_info = {};
	alloc_info.usage = vma_usage;

	VkResult result = vmaCreateImage(g_allocator, create_info, &alloc_info, out_image, out_alloc, nullptr);
	return result == VK_SUCCESS;
}

void VkAllocator::Destroy_Image(VkImage image, VmaAllocation alloc)
{
	if (g_allocator == VK_NULL_HANDLE) {
		return;
	}
	vmaDestroyImage(g_allocator, image, alloc);
}

void *VkAllocator::Map(VmaAllocation alloc)
{
	void *ptr = nullptr;
	vmaMapMemory(g_allocator, alloc, &ptr);
	return ptr;
}

void VkAllocator::Unmap(VmaAllocation alloc)
{
	vmaUnmapMemory(g_allocator, alloc);
}

} /* namespace ww3d_vulkan */
