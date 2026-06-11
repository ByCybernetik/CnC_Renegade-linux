#version 450

layout(constant_id = 0) const uint ALPHA_TEST_ENABLE = 0;

#include "frame_ubo.glsl"

layout(location = 0) in vec4 v_color;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec2 v_uv2;
layout(location = 3) in float v_fog_factor;
layout(location = 4) in vec3 v_normal_cs;

layout(set = 0, binding = 1) uniform sampler2D diffuse_tex;
layout(set = 0, binding = 2) uniform sampler2D detail_tex;

layout(location = 0) out vec4 out_color;

const uint FLAG_TEXTURING = 4u;
const uint FLAG_FOG = 16u;

vec2 Apply_Bump_Offset(vec2 uv, vec4 bump_texel)
{
	vec2 bump = bump_texel.rg * 2.0 - 1.0;
	float du = bump.x * ubo.bump_mat[0] + bump.y * ubo.bump_mat[1];
	float dv = bump.x * ubo.bump_mat[2] + bump.y * ubo.bump_mat[3];
	return uv + vec2(du, dv);
}

vec4 Apply_Stage0(vec4 base_color, vec4 texel, vec2 uv)
{
	float mode = ubo.tex_stage0_mode;
	if (mode < 0.5) {
		return vec4(texel.rgb, texel.a * base_color.a);
	}
	if (mode < 1.5) {
		return texel * base_color;
	}
	if (mode < 2.5) {
		return vec4(texel.rgb + base_color.rgb, texel.a * base_color.a);
	}
	if (mode < 3.5) {
		vec2 env_uv = Apply_Bump_Offset(uv, texel);
		vec4 env = texture(detail_tex, env_uv);
		return vec4(env.rgb * base_color.rgb, base_color.a);
	}
	if (mode < 4.5) {
		vec2 env_uv = Apply_Bump_Offset(uv, texel);
		vec4 env = texture(detail_tex, env_uv);
		float lum = texel.r * ubo.bump_l_scale + ubo.bump_l_offset;
		return vec4(env.rgb * lum * base_color.rgb, base_color.a);
	}
	vec3 perturbed = normalize(texel.rgb * 2.0 - 1.0);
	float dp = dot(perturbed, normalize(v_normal_cs));
	return vec4(base_color.rgb * dp, base_color.a);
}

vec4 Apply_Stage1Color(vec4 current, vec4 detail)
{
	float mode = ubo.tex_stage1_color_mode;
	if (mode < 0.5) {
		return current;
	}
	if (mode < 1.5) {
		return vec4(detail.rgb, current.a);
	}
	if (mode < 2.5) {
		return current * detail;
	}
	if (mode < 3.5) {
		return current + (vec4(1.0) - current) * detail;
	}
	if (mode < 4.5) {
		return current + detail;
	}
	if (mode < 5.5) {
		return current - detail;
	}
	if (mode < 6.5) {
		return detail - current;
	}
	if (mode < 7.5) {
		return detail.a * detail + (1.0 - detail.a) * current;
	}
	return detail.a * current + (1.0 - detail.a) * detail;
}

float Apply_Stage1Alpha(float current, float detail_a)
{
	float mode = ubo.tex_stage1_alpha_mode;
	if (mode < 0.5) {
		return current;
	}
	if (mode < 1.5) {
		return detail_a;
	}
	if (mode < 2.5) {
		return current * detail_a;
	}
	return current + (1.0 - current) * detail_a;
}

void main()
{
	uint flags = uint(ubo.flags);
	vec4 color = v_color;

	if ((flags & FLAG_TEXTURING) != 0u) {
		vec4 texel = texture(diffuse_tex, v_uv);
		color = Apply_Stage0(color, texel, v_uv);
	}

	if ((flags & FLAG_TEXTURING) != 0u &&
			(ubo.tex_stage1_color_mode > 0.5 || ubo.tex_stage1_alpha_mode > 0.5)) {
		vec4 detail = texture(detail_tex, v_uv2);
		color = Apply_Stage1Color(color, detail);
		color.a = Apply_Stage1Alpha(color.a, detail.a);
	}

	if ((flags & FLAG_FOG) != 0u) {
		float f = v_fog_factor;
		if (ubo.fog_mode < 0.5) {
			color.rgb = mix(ubo.fog_color.rgb, color.rgb, f);
		} else if (ubo.fog_mode < 1.5) {
			color.rgb *= f;
		} else {
			color.rgb = mix(color.rgb, ubo.fog_color.rgb, 1.0 - f);
		}
	}

	out_color = color;

	if (ALPHA_TEST_ENABLE != 0u && out_color.a < 0.5) {
		discard;
	}
}
