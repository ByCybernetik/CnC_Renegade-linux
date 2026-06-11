#ifndef TEXVIEW_RENDERER_H
#define TEXVIEW_RENDERER_H

#include "../ww3d2_vulkan/vk_texture.h"
#include "../ww3d2_vulkan/vk_swapchain.h"

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <vector>

enum TexViewScaleMode {
	TEXVIEW_SCALE_FIT = 0,
	TEXVIEW_SCALE_ONE_TO_ONE,
};

class TexViewRenderer {
public:
	bool Init(SDL_Window *window, uint32_t width, uint32_t height);
	void Shutdown();

	void Resize(uint32_t width, uint32_t height);
	bool Begin_Frame();
	bool End_Frame();

	void Set_Texture(const ww3d_vulkan::VkTexture *texture);
	void Set_Scale_Mode(TexViewScaleMode mode) { scale_mode_ = mode; }
	void Set_Show_Alpha_Background(bool show) { show_alpha_bg_ = show; }

private:
	bool Load_Shaders();
	bool Create_Render_Pass();
	bool Create_Framebuffers();
	bool Create_Pipeline();
	bool Create_Vertex_Buffer();
	bool Create_Descriptor_Pool();
	bool Create_Sync_Objects();
	void Destroy_Pipeline();
	void Destroy_Framebuffers();
	void Recreate_Swapchain();
	void Update_Quad();
	void Record_Draw(VkCommandBuffer cmd);

	SDL_Window *window_ = nullptr;
	ww3d_vulkan::VkSwapchain swapchain_;
	VkFormat swapchain_format_ = VK_FORMAT_UNDEFINED;
	VkExtent2D extent_ = {};

	VkRenderPass render_pass_ = VK_NULL_HANDLE;
	std::vector<VkFramebuffer> framebuffers_;

	VkShaderModule vert_shader_ = VK_NULL_HANDLE;
	VkShaderModule frag_shader_ = VK_NULL_HANDLE;
	VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
	VkPipeline pipeline_ = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptor_layout_ = VK_NULL_HANDLE;
	VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
	VkDescriptorSet descriptor_set_ = VK_NULL_HANDLE;

	VkBuffer vertex_buffer_ = VK_NULL_HANDLE;
	VkDeviceMemory vertex_memory_ = VK_NULL_HANDLE;

	VkSemaphore image_available_ = VK_NULL_HANDLE;
	VkSemaphore render_finished_ = VK_NULL_HANDLE;
	VkFence in_flight_ = VK_NULL_HANDLE;
	VkCommandBuffer command_buffer_ = VK_NULL_HANDLE;

	std::vector<uint32_t> vert_spirv_;
	std::vector<uint32_t> frag_spirv_;

	const ww3d_vulkan::VkTexture *texture_ = nullptr;
	TexViewScaleMode scale_mode_ = TEXVIEW_SCALE_FIT;
	bool show_alpha_bg_ = true;

	uint32_t current_image_ = 0;
	bool frame_active_ = false;
	bool vsync_ = true;
};

#endif
