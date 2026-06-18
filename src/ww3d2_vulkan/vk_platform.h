#ifndef WW3D2_VULKAN_VK_PLATFORM_H
#define WW3D2_VULKAN_VK_PLATFORM_H

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <cstdint>
#include <vector>

namespace ww3d_vulkan {

constexpr uint32_t kMaxFramesInFlight = 2;
constexpr uint32_t kUboDrawsPerFrame = 8192;

struct FrameSync {
	VkSemaphore image_available = VK_NULL_HANDLE;
	VkSemaphore render_finished = VK_NULL_HANDLE;
	VkFence in_flight = VK_NULL_HANDLE;
	VkCommandBuffer command_buffer = VK_NULL_HANDLE;
};

} /* namespace ww3d_vulkan */

#endif
