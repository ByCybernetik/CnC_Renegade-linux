#ifndef WW3D2_VULKAN_WW3D_VULKAN_H
#define WW3D2_VULKAN_WW3D_VULKAN_H

/*
 * Native Vulkan backend for WW3D2 (Linux).
 * Replaces dxvk-native / D3D8 device when built with -DRENEGADE_VULKAN.
 */

#include "vk_pipeline.h"
#include "vk_renderer.h"
#include "vk_texture.h"
#include "../ww3d2/ww3dformat.h"
#include <vulkan/vulkan.h>

struct SDL_Window;

namespace ww3d_vulkan {

class WW3DVulkan {
public:
	static WW3DVulkan &Get();

	bool Init(SDL_Window *window, uint32_t width, uint32_t height, bool vsync);
	void Shutdown();
	bool Is_Active() const { return active_; }

	bool Create_Device(uint32_t width, uint32_t height, bool windowed, bool vsync);
	void Release_Device();

	bool Begin_Scene();
	bool End_Scene(bool present);

	void Clear(bool color, bool depth, float r, float g, float b, float a);
	void Set_Viewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
	void Resize(uint32_t width, uint32_t height);

	void Set_View_Projection(const float matrix[16]);
	void Set_World_Matrix(const float matrix[16]);
	void Set_View_Matrix(const float matrix[16]);
	void Set_Lighting_State(const FrameUBO &state);
	void Bind_Texture(unsigned stage, VkTexture *texture);

	void Draw_Indexed(
		VkBuffer vb,
		VkBuffer ib,
		uint32_t index_count,
		uint32_t first_index,
		uint32_t vertex_offset,
		const MeshPipelineKey &key);

	VkRenderer &Renderer() { return renderer_; }


private:
	WW3DVulkan() = default;

	bool active_ = false;
	bool scene_open_ = false;
	uint32_t width_ = 800;
	uint32_t height_ = 600;
	bool vsync_ = true;
	float clear_r_ = 0.0f;
	float clear_g_ = 0.0f;
	float clear_b_ = 0.0f;
	float clear_a_ = 1.0f;
	bool clear_color_ = true;
	bool clear_depth_ = true;
	VkRenderer renderer_;
};

bool Is_Enabled();
bool Try_Create_Device(uint32_t width, uint32_t height, bool windowed, bool vsync);
void Try_Release_Device();
bool Try_Begin_Scene();
bool Try_End_Scene(bool present);
void Try_Clear(bool color, bool depth, float r, float g, float b, float a);
void Try_Sync_Matrices(const float world[16], const float view_proj[16]);
void Try_Set_Viewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void Try_Resize(uint32_t width, uint32_t height);
void Try_Set_Render_Target(VkTexture *target);
VkTexture *Try_Create_Render_Target(uint32_t width, uint32_t height, WW3DFormat format);

} /* namespace ww3d_vulkan */

#endif
