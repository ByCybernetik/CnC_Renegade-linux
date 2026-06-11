#ifndef WW3D2_VULKAN_VK_CONTEXT_H
#define WW3D2_VULKAN_VK_CONTEXT_H

#include "vk_platform.h"
#include <vector>

namespace ww3d_vulkan {

class VkContext {
public:
	static VkContext &Get();

	bool Init(SDL_Window *window, bool enable_validation);
	void Shutdown();

	bool Is_Ready() const { return device_ != VK_NULL_HANDLE; }

	VkInstance Instance() const { return instance_; }
	VkPhysicalDevice Physical_Device() const { return physical_device_; }
	VkDevice Device() const { return device_; }
	VkQueue Graphics_Queue() const { return graphics_queue_; }
	VkQueue Present_Queue() const { return present_queue_; }
	uint32_t Graphics_Queue_Family() const { return graphics_queue_family_; }
	uint32_t Present_Queue_Family() const { return present_queue_family_; }
	VkSurfaceKHR Surface() const { return surface_; }
	VkCommandPool Command_Pool() const { return command_pool_; }
	VkPhysicalDeviceProperties const &Device_Properties() const { return device_properties_; }

private:
	VkContext() = default;

	bool Create_Instance(bool enable_validation);
	bool Pick_Physical_Device();
	bool Create_Logical_Device();
	bool Create_Surface(SDL_Window *window);
	bool Create_Command_Pool();

	VkInstance instance_ = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
	VkSurfaceKHR surface_ = VK_NULL_HANDLE;
	VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
	VkDevice device_ = VK_NULL_HANDLE;
	VkQueue graphics_queue_ = VK_NULL_HANDLE;
	VkQueue present_queue_ = VK_NULL_HANDLE;
	uint32_t graphics_queue_family_ = 0;
	uint32_t present_queue_family_ = 0;
	VkCommandPool command_pool_ = VK_NULL_HANDLE;
	VkPhysicalDeviceProperties device_properties_ = {};
	std::vector<const char *> instance_extensions_;
	std::vector<const char *> device_extensions_;
};

} /* namespace ww3d_vulkan */

#endif
