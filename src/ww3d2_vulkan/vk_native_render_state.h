#ifndef WW3D2_VULKAN_NATIVE_RENDER_STATE_H
#define WW3D2_VULKAN_NATIVE_RENDER_STATE_H

#if defined(RENEGADE_VULKAN)

struct Matrix4;
class ShaderClass;

namespace ww3d_vulkan {

struct NativeTextureState;
struct VulkanDrawState;

/* Per-stage native UV / texture-matrix state (Phase 1 — replaces D3DTSS at draw time). */
void Native_Render_State_Reset();
void Native_Render_State_On_Tss(unsigned stage, unsigned tss, unsigned value);
void Native_Render_State_Sync_Stage_Matrix(
	unsigned stage,
	const Matrix4 &tex_mat,
	unsigned texture_transform_flags);
void Native_Render_State_On_Shader(const ShaderClass &shader);
void Native_Render_State_Fill_Texture(VulkanDrawState *state);

/* UI draw override (priority over per-stage registry). */
void Push_Native_Texture_State(const NativeTextureState &state);
void Clear_Native_Texture_State();
bool Apply_Native_Texture_State(VulkanDrawState *state);

} /* namespace ww3d_vulkan */

#endif /* RENEGADE_VULKAN */

#endif
