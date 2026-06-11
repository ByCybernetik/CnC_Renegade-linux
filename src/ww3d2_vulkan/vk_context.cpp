#include "vk_context.h"
#include "vk_allocator.h"
#include "vk_check.h"

#include <climits>
#include <cstring>
#include <set>
#include <vector>

namespace ww3d_vulkan {

namespace {

#if !defined(NDEBUG)
static VKAPI_ATTR VkBool32 VKAPI_CALL Debug_Callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT type,
	const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
	void *user_data)
{
	(void)type;
	(void)user_data;
	if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		fprintf(stderr, "Vulkan validation: %s\n", callback_data->pMessage);
	}
	return VK_FALSE;
}
#endif

static bool Has_Extension(
	const std::vector<VkExtensionProperties> &available,
	const char *name)
{
	for (size_t i = 0; i < available.size(); ++i) {
		if (strcmp(available[i].extensionName, name) == 0) {
			return true;
		}
	}
	return false;
}

} /* namespace */

VkContext &VkContext::Get()
{
	static VkContext instance;
	return instance;
}

bool VkContext::Init(SDL_Window *window, bool enable_validation)
{
	if (device_ != VK_NULL_HANDLE) {
		return true;
	}

	if (!Create_Instance(enable_validation)) {
		return false;
	}
	if (!Create_Surface(window)) {
		Shutdown();
		return false;
	}
	if (!Pick_Physical_Device()) {
		Shutdown();
		return false;
	}
	if (!Create_Logical_Device()) {
		Shutdown();
		return false;
	}
	if (!VkAllocator::Init(instance_, physical_device_, device_, VK_API_VERSION_1_2)) {
		Shutdown();
		return false;
	}
	if (!Create_Command_Pool()) {
		Shutdown();
		return false;
	}

	return true;
}

void VkContext::Shutdown()
{
	if (device_ != VK_NULL_HANDLE) {
		vkDeviceWaitIdle(device_);
	}

	VkAllocator::Shutdown();

	if (command_pool_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
		vkDestroyCommandPool(device_, command_pool_, nullptr);
		command_pool_ = VK_NULL_HANDLE;
	}

	if (device_ != VK_NULL_HANDLE) {
		vkDestroyDevice(device_, nullptr);
		device_ = VK_NULL_HANDLE;
	}

	if (surface_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(instance_, surface_, nullptr);
		surface_ = VK_NULL_HANDLE;
	}

#if !defined(NDEBUG)
	if (debug_messenger_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
		auto destroy_fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
			vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
		if (destroy_fn != nullptr) {
			destroy_fn(instance_, debug_messenger_, nullptr);
		}
		debug_messenger_ = VK_NULL_HANDLE;
	}
#endif

	if (instance_ != VK_NULL_HANDLE) {
		vkDestroyInstance(instance_, nullptr);
		instance_ = VK_NULL_HANDLE;
	}

	physical_device_ = VK_NULL_HANDLE;
	graphics_queue_ = VK_NULL_HANDLE;
	present_queue_ = VK_NULL_HANDLE;
}

bool VkContext::Create_Instance(bool enable_validation)
{
	uint32_t sdl_extension_count = 0;
	const char *const *sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_extension_count);
	if (sdl_extensions == nullptr || sdl_extension_count == 0) {
		fprintf(stderr, "SDL_Vulkan_GetInstanceExtensions failed: %s\n", SDL_GetError());
		return false;
	}

	instance_extensions_.clear();
	for (uint32_t i = 0; i < sdl_extension_count; ++i) {
		instance_extensions_.push_back(sdl_extensions[i]);
	}

#if !defined(NDEBUG)
	if (enable_validation) {
		instance_extensions_.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
#endif

	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "CnC Renegade";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "WW3D2 Vulkan";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_2;

	VkInstanceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;
	create_info.enabledExtensionCount = (uint32_t)instance_extensions_.size();
	create_info.ppEnabledExtensionNames = instance_extensions_.data();

#if !defined(NDEBUG)
	const char *validation_layers[] = {"VK_LAYER_KHRONOS_validation"};
	if (enable_validation) {
		create_info.enabledLayerCount = 1;
		create_info.ppEnabledLayerNames = validation_layers;
	}
#endif

	VK_CHECK(vkCreateInstance(&create_info, nullptr, &instance_));

#if !defined(NDEBUG)
	if (enable_validation) {
		VkDebugUtilsMessengerCreateInfoEXT debug_info = {};
		debug_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debug_info.messageSeverity =
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debug_info.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debug_info.pfnUserCallback = Debug_Callback;

		auto create_fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
			vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
		if (create_fn != nullptr) {
			create_fn(instance_, &debug_info, nullptr, &debug_messenger_);
		}
	}
#endif

	return true;
}

bool VkContext::Create_Surface(SDL_Window *window)
{
	if (!SDL_Vulkan_CreateSurface(window, instance_, nullptr, &surface_)) {
		fprintf(stderr, "SDL_Vulkan_CreateSurface failed: %s\n", SDL_GetError());
		return false;
	}
	return true;
}

bool VkContext::Pick_Physical_Device()
{
	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);
	if (device_count == 0) {
		fprintf(stderr, "No Vulkan physical devices found.\n");
		return false;
	}

	std::vector<VkPhysicalDevice> devices(device_count);
	vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());

	for (uint32_t i = 0; i < device_count; ++i) {
		uint32_t queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_family_count, nullptr);
		std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(
			devices[i], &queue_family_count, queue_families.data());

		uint32_t graphics_family = UINT32_MAX;
		uint32_t present_family = UINT32_MAX;
		for (uint32_t q = 0; q < queue_family_count; ++q) {
			if (queue_families[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				graphics_family = q;
			}
			VkBool32 present_support = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(devices[i], q, surface_, &present_support);
			if (present_support == VK_TRUE) {
				present_family = q;
			}
		}

		if (graphics_family == UINT32_MAX || present_family == UINT32_MAX) {
			continue;
		}

		uint32_t extension_count = 0;
		vkEnumerateDeviceExtensionProperties(devices[i], nullptr, &extension_count, nullptr);
		std::vector<VkExtensionProperties> extensions(extension_count);
		vkEnumerateDeviceExtensionProperties(
			devices[i], nullptr, &extension_count, extensions.data());

		if (!Has_Extension(extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
			continue;
		}

		physical_device_ = devices[i];
		graphics_queue_family_ = graphics_family;
		present_queue_family_ = present_family;
		vkGetPhysicalDeviceProperties(physical_device_, &device_properties_);
		return true;
	}

	fprintf(stderr, "No suitable Vulkan GPU found.\n");
	return false;
}

bool VkContext::Create_Logical_Device()
{
	device_extensions_.clear();
	device_extensions_.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	device_extensions_.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

	std::set<uint32_t> unique_queue_families;
	unique_queue_families.insert(graphics_queue_family_);
	unique_queue_families.insert(present_queue_family_);

	float queue_priority = 1.0f;
	std::vector<VkDeviceQueueCreateInfo> queue_infos;
	for (uint32_t family : unique_queue_families) {
		VkDeviceQueueCreateInfo queue_info = {};
		queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_info.queueFamilyIndex = family;
		queue_info.queueCount = 1;
		queue_info.pQueuePriorities = &queue_priority;
		queue_infos.push_back(queue_info);
	}

	VkPhysicalDeviceFeatures device_features = {};
	device_features.fillModeNonSolid = VK_TRUE;
	device_features.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.queueCreateInfoCount = (uint32_t)queue_infos.size();
	create_info.pQueueCreateInfos = queue_infos.data();
	create_info.pEnabledFeatures = &device_features;
	create_info.enabledExtensionCount = (uint32_t)device_extensions_.size();
	create_info.ppEnabledExtensionNames = device_extensions_.data();

	VK_CHECK(vkCreateDevice(physical_device_, &create_info, nullptr, &device_));
	vkGetDeviceQueue(device_, graphics_queue_family_, 0, &graphics_queue_);
	vkGetDeviceQueue(device_, present_queue_family_, 0, &present_queue_);
	return true;
}

bool VkContext::Create_Command_Pool()
{
	VkCommandPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = graphics_queue_family_;
	VK_CHECK(vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_));
	return true;
}

} /* namespace ww3d_vulkan */
