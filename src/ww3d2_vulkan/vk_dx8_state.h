#ifndef WW3D2_VULKAN_VK_DX8_STATE_H
#define WW3D2_VULKAN_VK_DX8_STATE_H

#if defined(RENEGADE_VULKAN)

#include "vk_renderer.h"
#include <stdint.h>

namespace ww3d_vulkan {

struct VulkanDrawState {
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
	uint32_t flags;
	float tex_stage0_mode;
	float tex_stage1_color_mode;
	float tex_stage1_alpha_mode;
	float material_shininess;
	float specular_enable;
	float bump_mat[4];
	float bump_l_scale;
	float bump_l_offset;
	float tex_tci[2];
	float tex_uv_index[2];
	float tex_mat[2][4];
};

void Fill_Vulkan_Draw_State(VulkanDrawState *state);
void Sync_Draw_State();

} /* namespace ww3d_vulkan */

#endif

#endif
