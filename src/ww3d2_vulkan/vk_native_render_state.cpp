#include "vk_native_render_state.h"

#if defined(RENEGADE_VULKAN)

#include "render_device.h"
#include "vk_dx8_state.h"
#include "../ww3d2/dx8wrapper.h"
#include "../ww3d2/shader.h"
#include "../wwmath/matrix4.h"

#include <d3d8.h>

namespace ww3d_vulkan {

namespace {

struct StageNative {
	UvMode uv_mode = UvMode::Passthru;
	uint8_t uv_index = 0;
	unsigned ttf = D3DTTFF_DISABLE;
	float tex_mat[4] = {1.0f, 1.0f, 0.0f, 0.0f};
};

static const unsigned kMaxStages = 2;

StageNative g_stage[kMaxStages];
float g_tex_stage0_mode = 1.0f;
float g_tex_stage1_color_mode = 0.0f;
float g_tex_stage1_alpha_mode = 0.0f;
float g_bump_mat[4] = {0.0f, 0.0f, 0.0f, 0.0f};
float g_bump_l_scale = 0.0f;
float g_bump_l_offset = 0.0f;

static NativeTextureState g_texture_override;
static bool g_texture_override_active = false;

static UvMode Tci_To_UvMode(unsigned tci)
{
	const unsigned flags = (tci >> 16) & 0xFFFFu;
	switch (flags) {
	case 0:
		return UvMode::Passthru;
	case 1:
		return UvMode::CameraNormal;
	case 2:
		return UvMode::CameraReflection;
	case 3:
		/* Projective / camera-space position — no dedicated shader path yet. */
		return UvMode::SphereMap;
	default:
		return UvMode::Passthru;
	}
}

static float Shader_Gradient_To_Stage0(const ShaderClass &shader)
{
	switch (shader.Get_Primary_Gradient()) {
	case ShaderClass::GRADIENT_DISABLE:
		return 0.0f;
	case ShaderClass::GRADIENT_ADD:
		return 2.0f;
	case ShaderClass::GRADIENT_BUMPENVMAP:
		return 3.0f;
	case ShaderClass::GRADIENT_BUMPENVMAPLUMINANCE:
		return 4.0f;
	case ShaderClass::GRADIENT_DOTPRODUCT3:
		return 5.0f;
	default:
		return 1.0f;
	}
}

static void Extract_Tex_Mat(const Matrix4 &tex_mat, float out[4])
{
	out[0] = tex_mat[0][0];
	out[1] = tex_mat[1][1];
	out[2] = tex_mat[0][2];
	out[3] = tex_mat[1][2];
}

static void Apply_Stage_To_Draw_State(
	VulkanDrawState *state,
	unsigned stage,
	const StageNative &native)
{
	state->tex_uv_index[stage] = (float)native.uv_index;
	if (native.ttf == D3DTTFF_DISABLE) {
		state->tex_tci[stage] = 0.0f;
		state->tex_mat[stage][0] = 1.0f;
		state->tex_mat[stage][1] = 1.0f;
		state->tex_mat[stage][2] = 0.0f;
		state->tex_mat[stage][3] = 0.0f;
	} else {
		state->tex_tci[stage] = (float)static_cast<uint8_t>(native.uv_mode);
		state->tex_mat[stage][0] = native.tex_mat[0];
		state->tex_mat[stage][1] = native.tex_mat[1];
		state->tex_mat[stage][2] = native.tex_mat[2];
		state->tex_mat[stage][3] = native.tex_mat[3];
	}
}

static void Apply_Override_To_Draw_State(
	VulkanDrawState *state,
	const NativeTextureState &native)
{
	state->tex_stage0_mode = native.tex_stage0_mode;
	state->tex_stage1_color_mode = native.tex_stage1_color_mode;
	state->tex_stage1_alpha_mode = native.tex_stage1_alpha_mode;
	state->bump_mat[0] = 0.0f;
	state->bump_mat[1] = 0.0f;
	state->bump_mat[2] = 0.0f;
	state->bump_mat[3] = 0.0f;
	state->bump_l_scale = 0.0f;
	state->bump_l_offset = 0.0f;

	for (unsigned stage = 0; stage < kMaxStages; ++stage) {
		state->tex_tci[stage] = (float)static_cast<uint8_t>(native.uv_mode[stage]);
		state->tex_uv_index[stage] = (float)native.uv_index[stage];
		state->tex_mat[stage][0] = native.tex_mat[stage][0];
		state->tex_mat[stage][1] = native.tex_mat[stage][1];
		state->tex_mat[stage][2] = native.tex_mat[stage][2];
		state->tex_mat[stage][3] = native.tex_mat[stage][3];
	}
}

} /* namespace */

void Native_Render_State_Reset()
{
	for (unsigned i = 0; i < kMaxStages; ++i) {
		g_stage[i] = StageNative();
	}
	g_tex_stage0_mode = 1.0f;
	g_tex_stage1_color_mode = 0.0f;
	g_tex_stage1_alpha_mode = 0.0f;
	g_bump_mat[0] = g_bump_mat[1] = g_bump_mat[2] = g_bump_mat[3] = 0.0f;
	g_bump_l_scale = 0.0f;
	g_bump_l_offset = 0.0f;
	g_texture_override_active = false;
}

void Native_Render_State_On_Tss(unsigned stage, unsigned tss, unsigned value)
{
	if (stage >= kMaxStages) {
		return;
	}

	StageNative &native = g_stage[stage];
	switch (tss) {
	case D3DTSS_TEXCOORDINDEX:
		native.uv_index = (uint8_t)(value & 0xFFFFu);
		native.uv_mode = Tci_To_UvMode(value);
		break;
	case D3DTSS_TEXTURETRANSFORMFLAGS:
		native.ttf = value;
		break;
	case D3DTSS_BUMPENVMAT00:
		g_bump_mat[0] = *reinterpret_cast<const float *>(&value);
		break;
	case D3DTSS_BUMPENVMAT01:
		g_bump_mat[1] = *reinterpret_cast<const float *>(&value);
		break;
	case D3DTSS_BUMPENVMAT10:
		g_bump_mat[2] = *reinterpret_cast<const float *>(&value);
		break;
	case D3DTSS_BUMPENVMAT11:
		g_bump_mat[3] = *reinterpret_cast<const float *>(&value);
		break;
	case D3DTSS_BUMPENVLSCALE:
		g_bump_l_scale = *reinterpret_cast<const float *>(&value);
		break;
	case D3DTSS_BUMPENVLOFFSET:
		g_bump_l_offset = *reinterpret_cast<const float *>(&value);
		break;
	default:
		break;
	}
}

void Native_Render_State_Sync_Stage_Matrix(
	unsigned stage,
	const Matrix4 &tex_mat,
	unsigned texture_transform_flags)
{
	if (stage >= kMaxStages) {
		return;
	}

	g_stage[stage].ttf = texture_transform_flags;
	Extract_Tex_Mat(tex_mat, g_stage[stage].tex_mat);
}

void Native_Render_State_On_Shader(const ShaderClass &shader)
{
	if (shader.Get_Texturing() == ShaderClass::TEXTURING_ENABLE) {
		g_tex_stage0_mode = Shader_Gradient_To_Stage0(shader);
	} else {
		switch (shader.Get_Primary_Gradient()) {
		case ShaderClass::GRADIENT_DISABLE:
			g_tex_stage0_mode = 0.0f;
			break;
		default:
			g_tex_stage0_mode = 0.0f;
			break;
		}
	}
	g_tex_stage1_color_mode = (float)shader.Get_Post_Detail_Color_Func();
	g_tex_stage1_alpha_mode = (float)shader.Get_Post_Detail_Alpha_Func();
}

void Native_Render_State_Fill_Texture(VulkanDrawState *state)
{
	if (state == nullptr) {
		return;
	}

	state->tex_stage0_mode = g_tex_stage0_mode;
	state->tex_stage1_color_mode = g_tex_stage1_color_mode;
	state->tex_stage1_alpha_mode = g_tex_stage1_alpha_mode;
	state->bump_mat[0] = g_bump_mat[0];
	state->bump_mat[1] = g_bump_mat[1];
	state->bump_mat[2] = g_bump_mat[2];
	state->bump_mat[3] = g_bump_mat[3];
	state->bump_l_scale = g_bump_l_scale;
	state->bump_l_offset = g_bump_l_offset;

	for (unsigned stage = 0; stage < kMaxStages; ++stage) {
		Apply_Stage_To_Draw_State(state, stage, g_stage[stage]);
	}

	if (g_texture_override_active) {
		Apply_Override_To_Draw_State(state, g_texture_override);
	}
}

void Push_Native_Texture_State(const NativeTextureState &state)
{
	g_texture_override = state;
	g_texture_override_active = true;
}

void Clear_Native_Texture_State()
{
	g_texture_override_active = false;
}

bool Apply_Native_Texture_State(VulkanDrawState *state)
{
	if (!g_texture_override_active || state == nullptr) {
		return false;
	}
	Apply_Override_To_Draw_State(state, g_texture_override);
	return true;
}

} /* namespace ww3d_vulkan */

#endif /* RENEGADE_VULKAN */
