#include "ww3d_vulkan.h"
#include "../platform/sdl3_host.h"
#include <cstdio>

namespace ww3d_vulkan {

WW3DVulkan &WW3DVulkan::Get()
{
	static WW3DVulkan instance;
	return instance;
}

bool Is_Enabled()
{
#if defined(RENEGADE_VULKAN)
	return true;
#else
	return false;
#endif
}

bool WW3DVulkan::Init(SDL_Window *window, uint32_t width, uint32_t height, bool vsync)
{
	width_ = width;
	height_ = height;
	vsync_ = vsync;
	active_ = renderer_.Init(window, width, height, vsync);
	if (active_) {
		fprintf(stderr, "WW3DVulkan: initialized %ux%u\n", width, height);
	}
	return active_;
}

void WW3DVulkan::Shutdown()
{
	Release_Device();
	active_ = false;
}

bool WW3DVulkan::Create_Device(uint32_t width, uint32_t height, bool windowed, bool vsync)
{
	(void)windowed;
	SDL_Window *window = Platform_Get_SDL_Window();
	if (window == nullptr) {
		fprintf(stderr, "WW3DVulkan: SDL window not available.\n");
		return false;
	}
	return Init(window, width, height, vsync);
}

void WW3DVulkan::Release_Device()
{
	if (scene_open_) {
		End_Scene(false);
	}
	renderer_.Shutdown();
	active_ = false;
}

bool WW3DVulkan::Begin_Scene()
{
	if (!active_) {
		return false;
	}
	scene_open_ = renderer_.Begin_Frame(
		clear_color_ ? clear_r_ : 0.0f,
		clear_color_ ? clear_g_ : 0.0f,
		clear_color_ ? clear_b_ : 0.0f,
		clear_color_ ? clear_a_ : 1.0f);
	return scene_open_;
}

bool WW3DVulkan::End_Scene(bool present)
{
	if (!active_ || !scene_open_) {
		return false;
	}
	bool ok = renderer_.End_Frame(present);
	scene_open_ = false;
	return ok;
}

void WW3DVulkan::Clear(bool color, bool depth, float r, float g, float b, float a)
{
	clear_color_ = color;
	clear_depth_ = depth;
	clear_r_ = r;
	clear_g_ = g;
	clear_b_ = b;
	clear_a_ = a;
	if (scene_open_) {
		renderer_.Clear_During_Frame(color, depth, r, g, b, a);
	}
}

void WW3DVulkan::Set_Viewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
	(void)x;
	(void)y;
	renderer_.Set_Viewport(x, y, w, h);
}

void WW3DVulkan::Resize(uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0) {
		return;
	}
	width_ = width;
	height_ = height;
	renderer_.Resize(width_, height_);
}

void WW3DVulkan::Set_View_Projection(const float matrix[16])
{
	renderer_.Set_View_Projection(matrix);
}

void WW3DVulkan::Set_World_Matrix(const float matrix[16])
{
	renderer_.Set_World_Matrix(matrix);
}

void WW3DVulkan::Set_View_Matrix(const float matrix[16])
{
	renderer_.Set_View_Matrix(matrix);
}

void WW3DVulkan::Set_Lighting_State(const FrameUBO &state)
{
	renderer_.Set_Lighting_State(state);
}

void WW3DVulkan::Bind_Texture(unsigned stage, VkTexture *texture)
{
	renderer_.Bind_Texture(stage, texture);
}

void WW3DVulkan::Draw_Indexed(
	VkBuffer vb,
	VkBuffer ib,
	uint32_t index_count,
	uint32_t first_index,
	uint32_t vertex_offset,
	const MeshPipelineKey &key)
{
	renderer_.Draw_Indexed(
		vb,
		ib,
		index_count,
		first_index,
		vertex_offset,
		key);
}

bool Try_Create_Device(uint32_t width, uint32_t height, bool windowed, bool vsync)
{
	if (!Is_Enabled()) {
		return false;
	}
	return WW3DVulkan::Get().Create_Device(width, height, windowed, vsync);
}

void Try_Release_Device()
{
	if (!Is_Enabled()) {
		return;
	}
	WW3DVulkan::Get().Release_Device();
}

bool Try_Begin_Scene()
{
	if (!Is_Enabled()) {
		return false;
	}
	return WW3DVulkan::Get().Begin_Scene();
}

bool Try_End_Scene(bool present)
{
	if (!Is_Enabled()) {
		return false;
	}
	return WW3DVulkan::Get().End_Scene(present);
}

void Try_Clear(bool color, bool depth, float r, float g, float b, float a)
{
	if (!Is_Enabled()) {
		return;
	}
	WW3DVulkan::Get().Clear(color, depth, r, g, b, a);
}

void Try_Sync_Matrices(const float world[16], const float view_proj[16])
{
	if (!Is_Enabled()) {
		return;
	}
	WW3DVulkan::Get().Set_World_Matrix(world);
	WW3DVulkan::Get().Set_View_Projection(view_proj);
}

void Try_Set_Viewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
	if (!Is_Enabled() || !WW3DVulkan::Get().Is_Active()) {
		return;
	}
	WW3DVulkan::Get().Renderer().Set_Viewport(x, y, w, h);
}

void Try_Resize(uint32_t width, uint32_t height)
{
	if (!Is_Enabled() || !WW3DVulkan::Get().Is_Active()) {
		return;
	}
	WW3DVulkan::Get().Resize(width, height);
}

void Try_Set_Render_Target(VkTexture *target)
{
	if (!Is_Enabled() || !WW3DVulkan::Get().Is_Active()) {
		return;
	}
	WW3DVulkan::Get().Renderer().Set_Render_Target(target);
}

VkTexture *Try_Create_Render_Target(uint32_t width, uint32_t height, WW3DFormat format)
{
	if (!Is_Enabled() || !WW3DVulkan::Get().Is_Active()) {
		return nullptr;
	}
	VkTexture *tex = new VkTexture();
	if (!tex->Create_As_Render_Target(
			width,
			height,
			format,
			WW3DVulkan::Get().Renderer().Offscreen_Render_Pass(),
			WW3DVulkan::Get().Renderer().Depth_Format())) {
		delete tex;
		return nullptr;
	}
	return tex;
}

} /* namespace ww3d_vulkan */
