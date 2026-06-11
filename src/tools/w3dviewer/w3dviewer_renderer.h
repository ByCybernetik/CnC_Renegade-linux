#ifndef W3DVIEWER_RENDERER_H
#define W3DVIEWER_RENDERER_H

#include "w3dviewer_texture.h"
#include "../ww3d2_vulkan/vk_swapchain.h"

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <string>
#include <vector>

struct W3DViewerMesh;
struct W3DViewerScene;

class W3DViewerRenderer {
public:
	bool Init(SDL_Window *window, uint32_t width, uint32_t height);
	void Shutdown();

	void Resize(uint32_t width, uint32_t height);
	bool Begin_Frame();
	void Set_Mesh(const W3DViewerMesh *mesh, const std::string &extra_tex_dir = std::string());
	void Set_Scene(const W3DViewerScene *scene, const std::string &extra_tex_dir = std::string());
	void Set_MVP(const float *mvp_4x4);
	bool End_Frame();

private:
	struct GpuDrawBatch {
		uint32_t index_offset = 0;
		uint32_t index_count = 0;
		VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
		bool use_texture = false;
		bool alpha_blend = false;
		bool alpha_test = false;
		bool depth_write = true;
	};

	struct GpuSubMesh {
		VkBuffer vertex_buffer = VK_NULL_HANDLE;
		VkDeviceMemory vertex_memory = VK_NULL_HANDLE;
		VkBuffer index_buffer = VK_NULL_HANDLE;
		VkDeviceMemory index_memory = VK_NULL_HANDLE;
		uint32_t index_count = 0;
		int32_t bone_index = 0;
		std::vector<GpuDrawBatch> draw_batches;
	};

	bool Load_Shaders();
	bool Create_Depth_Buffer();
	bool Create_Render_Pass();
	bool Create_Framebuffers();
	bool Create_Descriptor_Layout();
	bool Create_Descriptor_Pool(uint32_t set_count);
	bool Create_Pipelines();
	bool Create_Mesh_Buffers(const W3DViewerMesh *mesh);
	bool Upload_SubMesh_Geometry(const W3DViewerMesh *mesh, GpuSubMesh *out);
	void Build_SubMesh_Draw_Batches(
		const W3DViewerMesh *mesh,
		GpuSubMesh *out,
		const std::vector<int32_t> *texture_remap = nullptr);
	bool Create_Sync_Objects();
	bool Create_Fallback_Texture();
	bool Load_Mesh_Textures(const W3DViewerMesh *mesh, const std::string &extra_tex_dir);
	void Destroy_Pipeline();
	void Destroy_Descriptors();
	void Destroy_Framebuffers();
	void Destroy_Mesh_Buffers();
	void Destroy_Textures();
	void Recreate_Swapchain();
	void Record_Draw(VkCommandBuffer cmd);
	void Draw_Batches(
		VkCommandBuffer cmd,
		VkPipeline pipeline,
		const float *mvp,
		VkBuffer vertex_buffer,
		VkBuffer index_buffer,
		uint32_t index_count,
		const std::vector<GpuDrawBatch> &batches,
		bool alpha_blend_pass,
		bool alpha_test_flag);

	SDL_Window *window_ = nullptr;
	ww3d_vulkan::VkSwapchain swapchain_;
	VkFormat swapchain_format_ = VK_FORMAT_UNDEFINED;
	VkExtent2D extent_ = {};

	VkImage depth_image_ = VK_NULL_HANDLE;
	VkDeviceMemory depth_memory_ = VK_NULL_HANDLE;
	VkImageView depth_view_ = VK_NULL_HANDLE;
	VkFormat depth_format_ = VK_FORMAT_D32_SFLOAT;

	VkRenderPass render_pass_ = VK_NULL_HANDLE;
	std::vector<VkFramebuffer> framebuffers_;

	VkShaderModule vert_shader_ = VK_NULL_HANDLE;
	VkShaderModule frag_shader_ = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
	VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
	VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
	VkPipeline pipeline_opaque_ = VK_NULL_HANDLE;
	VkPipeline pipeline_alpha_ = VK_NULL_HANDLE;

	std::vector<GpuSubMesh> sub_meshes_;
	bool animated_scene_ = false;

	std::vector<GpuDrawBatch> draw_batches_;
	std::vector<std::unique_ptr<w3dviewer::GpuTexture>> textures_;
	std::vector<VkDescriptorSet> texture_descriptor_sets_;
	VkDescriptorSet fallback_descriptor_set_ = VK_NULL_HANDLE;
	w3dviewer::GpuTexture fallback_texture_;

	VkSemaphore image_available_ = VK_NULL_HANDLE;
	VkSemaphore render_finished_ = VK_NULL_HANDLE;
	VkFence in_flight_ = VK_NULL_HANDLE;
	VkCommandBuffer command_buffer_ = VK_NULL_HANDLE;

	std::vector<uint32_t> vert_spirv_;
	std::vector<uint32_t> frag_spirv_;

	const W3DViewerMesh *mesh_ = nullptr;
	const W3DViewerScene *scene_ = nullptr;
	float view_proj_[16];

	uint32_t current_image_ = 0;
	bool frame_active_ = false;
	bool vsync_ = true;
};

#endif
