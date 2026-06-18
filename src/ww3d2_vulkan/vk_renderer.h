#ifndef WW3D2_VULKAN_VK_RENDERER_H
#define WW3D2_VULKAN_VK_RENDERER_H

#include "vk_buffer.h"
#include "vk_framebuffer.h"
#include "vk_pipeline.h"
#include "vk_platform.h"
#include "vk_render_pass.h"
#include "vk_swapchain.h"
#include "vk_texture.h"
#include <cstring>
#include <vector>

namespace ww3d_vulkan {

struct FrameUBO {
	enum {
		FLAG_LIGHTING = 1u << 0,
		FLAG_DIFFUSE_FROM_VERTEX = 1u << 1,
		FLAG_TEXTURING = 1u << 2,
		FLAG_COLOR1_UNLIT_MODULATE = 1u << 3,
		FLAG_FOG = 1u << 4,
		FLAG_SCREEN_BLEND_UNLIT = 1u << 5,
		FLAG_SCREEN_BLEND_LIT = 1u << 6,
		FLAG_SCREEN_BLEND_EVALOGO = 1u << 7,
		FLAG_SCREEN_BLEND_GIZMO_DIM = 1u << 8,
	};

	enum {
		FOG_MODE_BLEND = 0,
		FOG_MODE_SCALE = 1,
		FOG_MODE_WHITE = 2,
	};

	enum {
		LIGHT_OFF = 0,
		LIGHT_DIRECTIONAL = 1,
		LIGHT_POINT = 2,
	};

	float view_proj[16];
	float world[16];
	float view[16];
	float material_ambient[4];
	float material_diffuse[4];
	float material_emissive[4];
	float material_specular[4];
	float scene_ambient[4];
	float fog_color[4];
	float light_dir_or_pos[4][4];
	float light_diffuse[4][4];
	float light_params[4][4];
	float fog_start;
	float fog_end;
	float fog_mode;
	float flags;
	float tex_stage0_mode;
	float tex_stage1_color_mode;
	float tex_stage1_alpha_mode;
	float material_shininess;
	float specular_enable;
	float _pad_after_specular[3];
	/* std140: each float in an array occupies a 16-byte slot */
	struct { float v; float _pad[3]; } bump_mat[4];
	float bump_l_scale;
	float bump_l_offset;
	float _pad_before_tex_tci[2];
	struct { float v; float _pad[3]; } tex_tci[2];
	struct { float v; float _pad[3]; } tex_uv_index[2];
	float tex_mat[2][4];
};

inline void FrameUBO_Pack_Tex_Arrays(
	FrameUBO *ubo,
	const float bump_mat[4],
	float bump_l_scale,
	float bump_l_offset,
	const float tex_tci[2],
	const float tex_uv_index[2],
	const float tex_mat[2][4])
{
	if (ubo == nullptr) {
		return;
	}
	for (int i = 0; i < 4; ++i) {
		ubo->bump_mat[i].v = bump_mat[i];
		ubo->bump_mat[i]._pad[0] = 0.0f;
		ubo->bump_mat[i]._pad[1] = 0.0f;
		ubo->bump_mat[i]._pad[2] = 0.0f;
	}
	ubo->bump_l_scale = bump_l_scale;
	ubo->bump_l_offset = bump_l_offset;
	for (int i = 0; i < 2; ++i) {
		ubo->tex_tci[i].v = tex_tci[i];
		ubo->tex_tci[i]._pad[0] = 0.0f;
		ubo->tex_tci[i]._pad[1] = 0.0f;
		ubo->tex_tci[i]._pad[2] = 0.0f;
		ubo->tex_uv_index[i].v = tex_uv_index[i];
		ubo->tex_uv_index[i]._pad[0] = 0.0f;
		ubo->tex_uv_index[i]._pad[1] = 0.0f;
		ubo->tex_uv_index[i]._pad[2] = 0.0f;
	}
	memcpy(ubo->tex_mat, tex_mat, sizeof(ubo->tex_mat));
}

class VkRenderer {
public:
	bool Init(SDL_Window *window, uint32_t width, uint32_t height, bool vsync);
	void Shutdown();

	void Resize(uint32_t width, uint32_t height);
	bool Begin_Frame(float clear_r, float clear_g, float clear_b, float clear_a);
	bool End_Frame(bool present);
	void Clear_During_Frame(bool clear_color, bool clear_depth, float r, float g, float b, float a);

	void Set_View_Projection(const float matrix[16]);
	void Set_World_Matrix(const float matrix[16]);
	void Set_View_Matrix(const float matrix[16]);
	void Set_Lighting_State(const FrameUBO &state);
	void Bind_Texture(unsigned stage, VkTexture *texture);
	void Set_Render_Target(VkTexture *target);
	void Set_Viewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h);

	VkRenderPass Offscreen_Render_Pass() const { return render_pass_.Offscreen_Handle(); }
	VkFormat Depth_Format() const { return render_pass_.Depth_Format(); }

	void Draw_Indexed(
		VkBuffer vertex_buffer,
		VkBuffer index_buffer,
		uint32_t index_count,
		uint32_t first_index,
		uint32_t vertex_offset,
		const MeshPipelineKey &key);

	void Flush_Pending_Draws();

	VkExtent2D Extent() const { return swapchain_.Extent(); }
	VkPipelineLayout Pipeline_Layout() const { return pipelines_.Layout(); }
	VkDescriptorSetLayout Descriptor_Set_Layout() const { return pipelines_.Descriptor_Set_Layout(); }
	uint32_t Current_Frame() const { return current_frame_; }

private:
	struct PendingDraw {
		MeshPipelineKey key;
		VkBuffer vertex_buffer;
		VkBuffer index_buffer;
		uint32_t index_count;
		uint32_t first_index;
		uint32_t vertex_offset;
		int depth_bias;
		FrameUBO ubo;
		VkTexture *textures[2];
	};

	bool Create_Frame_Resources();
	bool Create_Sync_Objects();
	bool Load_Shaders();
	bool Create_Default_Texture();

	void Flush_Push_Descriptors(VkCommandBuffer cmd, VkPipelineLayout layout, VkDeviceSize ubo_offset);
	void Apply_Viewport(VkCommandBuffer cmd, VkExtent2D extent);
	void Recreate_Swapchain_Resources();

	VkSwapchain swapchain_;
	VkRenderPassMgr render_pass_;
	VkFramebufferMgr framebuffers_;
	VkPipelineCache pipelines_;
	VkPipelineCache pipelines_offscreen_;
	FrameSync frames_[kMaxFramesInFlight];
	VkBufferAlloc frame_ubo_ring_[kMaxFramesInFlight];
	uint32_t frame_ubo_draw_count_ = 0;
	uint32_t ubo_alignment_ = 256;

	std::vector<uint32_t> vert_spirv_;
	std::vector<uint32_t> frag_spirv_;

	uint32_t current_frame_ = 0;
	uint32_t current_image_ = 0;
	uint32_t width_ = 0;
	uint32_t height_ = 0;
	bool vsync_ = true;
	bool frame_active_ = false;

	FrameUBO frame_ubo_ = {};
	VkTexture default_texture_;
	VkTexture *bound_textures_[2] = {};
	bool textures_dirty_ = false;
	VkTexture *offscreen_target_ = nullptr;
	bool explicit_viewport_ = false;
	VkViewport viewport_state_ = {};
	VkRect2D scissor_state_ = {};
	VkPipeline bound_pipeline_ = VK_NULL_HANDLE;
	PFN_vkCmdPushDescriptorSetKHR push_descriptor_set_ = nullptr;

	/* Draw batching: queued per frame, sorted and flushed at frame end. */
	std::vector<PendingDraw> pending_draws_;
};

} /* namespace ww3d_vulkan */

#endif
