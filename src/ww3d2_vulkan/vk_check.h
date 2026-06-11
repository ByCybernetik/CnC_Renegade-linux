#ifndef WW3D2_VULKAN_VK_CHECK_H
#define WW3D2_VULKAN_VK_CHECK_H

#include <cstdio>
#include <cstdlib>

#define VK_CHECK(expr) \
	do { \
		VkResult _vk_result = (expr); \
		if (_vk_result != VK_SUCCESS) { \
			fprintf( \
				stderr, \
				"Vulkan error %d at %s:%d: %s\n", \
				(int)_vk_result, \
				__FILE__, \
				__LINE__, \
				#expr); \
			std::abort(); \
		} \
	} while (0)

#endif
