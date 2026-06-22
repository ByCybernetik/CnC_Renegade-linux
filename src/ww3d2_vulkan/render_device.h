#ifndef WW3D2_VULKAN_RENDER_DEVICE_H
#define WW3D2_VULKAN_RENDER_DEVICE_H

#if defined(RENEGADE_VULKAN)

#include "vk_pipeline.h"
#include "vk_2d_renderer.h"
#include "../ww3d2/ww3dformat.h"
#include <stdint.h>

class TextureClass;
class ShaderClass;
class DX8VertexBufferClass;
class DX8IndexBufferClass;
struct Matrix4;

namespace ww3d_vulkan {

/*
 * Native texture-coordinate modes (replacement for D3DTSS_TCI_* during migration).
 */
enum class UvMode : uint8_t {
	Passthru = 0,
	CameraNormal = 1,
	CameraReflection = 2,
	SphereMap = 3,
};

struct ClearDesc {
	bool color = true;
	bool depth = true;
	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;
	float a = 1.0f;
};

struct ViewportDesc {
	uint32_t x = 0;
	uint32_t y = 0;
	uint32_t w = 0;
	uint32_t h = 0;
};

struct DrawIndexedDesc {
	unsigned primitive_type = 0;
	unsigned short start_index = 0;
	unsigned short polygon_count = 0;
	unsigned short vertex_offset = 0;
	unsigned short vertex_count = 0;
	const ShaderClass *shader = nullptr;
	DX8VertexBufferClass *vertex_buffer = nullptr;
	DX8IndexBufferClass *index_buffer = nullptr;
	unsigned fvf = 0;
};

/*
 * Native texture pipeline state (Phase 1). Replaces D3DTSS_* for UV generation
 * and stage combiners once pushed via Push_Native_Texture_State().
 */
struct NativeTextureState {
	UvMode uv_mode[2] = {UvMode::Passthru, UvMode::Passthru};
	uint8_t uv_index[2] = {0, 1};
	float tex_mat[2][4] = {{1.0f, 1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 0.0f}};
	float tex_stage0_mode = 1.0f;
	float tex_stage1_color_mode = 0.0f;
	float tex_stage1_alpha_mode = 0.0f;
};

struct Native2DBatchDesc {
	const Simple2DVertex *vertices;
	uint32_t vertex_count;
	const uint16_t *indices;
	uint32_t index_count;
	void *texture; // VkTexture* or nullptr
	bool texturing;
	uint8_t src_blend;
	uint8_t dst_blend;
	float modulate_color[4];
	uint32_t viewport_x;
	uint32_t viewport_y;
	uint32_t viewport_w;
	uint32_t viewport_h;
};

/*
 * Vulkan render device — target API replacing DX8Wrapper for GPU work.
 * Phase 4: Offscreen render targets / projectors via VulkanRenderDevice.
 */
class VulkanRenderDevice {
public:
	static VulkanRenderDevice &Get();

	bool Is_Active() const;

	bool Begin_Frame();
	bool End_Frame(bool present);

	/* Phase 3 — full frame bracket (WW3D::Begin_Render / End_Render). */
	void Begin_Render(
		bool clear_color,
		bool clear_depth,
		float clear_r,
		float clear_g,
		float clear_b);
	bool End_Render(bool present);

	void Clear(const ClearDesc &desc);
	void Set_Viewport(const ViewportDesc &desc);
	void Resize(uint32_t width, uint32_t height);

	/* Phase 4 — offscreen render targets (shadow / texture projectors). */
	TextureClass *Create_Render_Target(int width, int height, WW3DFormat format);
	void Set_Render_Target(TextureClass *texture);
	void Restore_Default_Render_Target();
	void Get_Render_Target_Resolution(int &width, int &height) const;
	bool Is_Render_To_Texture() const;

	/* Reset TCI/texture-matrix state for 2D/UI (passthru UV). */
	void Reset_Ui_Texture_Stages(bool unbind_textures = false);

	/*
	 * Sync matrices, textures, and draw-state UBO before vkCmdDrawIndexed.
	 * reset_ui_stages: true for dynamic-VB UI draws.
	 */
	void Prepare_Draw(
		bool reset_ui_stages,
		const ShaderClass &shader,
		const Matrix4 &world,
		const Matrix4 &view,
		const Matrix4 &projection,
		TextureClass *const *textures,
		unsigned texture_count);

	/* Build UI passthru texture state from ShaderClass (no D3DTSS). */
	static NativeTextureState Build_Ui_Texture_State(const ShaderClass &shader);

	bool Draw_Indexed(const DrawIndexedDesc &desc);

	/*
	 * Phase 2 — indexed draw funnel (replaces DX8Wrapper::Draw on Vulkan).
	 * Uses current DX8Wrapper render state (VB/IB/shader/textures).
	 */
	void Draw_Primitive(
		unsigned primitive_type,
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count);

	void Draw_Triangles(
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count);

	void Draw_Strip(
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count);

	/* Flush queued draws (e.g. after 3D menu backdrop before 2D UI). */
	void Flush_Pending_Draws();

	/* Native Vulkan 2D/UI draw path (bypasses D3D8/DX8Wrapper). */
	void Draw_2D_Batch(const Native2DBatchDesc &desc);

private:
	VulkanRenderDevice() = default;
};

} /* namespace ww3d_vulkan */

#endif /* RENEGADE_VULKAN */

#endif
