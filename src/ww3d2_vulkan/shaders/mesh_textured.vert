#version 450

#include "frame_ubo.glsl"

layout(constant_id = 1) const uint HAS_NORMAL = 1;
layout(constant_id = 2) const uint HAS_DIFFUSE = 1;
layout(constant_id = 3) const uint TEX_LAYER_COUNT = 2;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in uint in_diffuse;
layout(location = 3) in vec2 in_uv;
layout(location = 4) in vec2 in_uv2;

layout(location = 0) out vec4 v_color;
layout(location = 1) out vec2 v_uv;
layout(location = 2) out vec2 v_uv2;
layout(location = 3) out float v_fog_factor;
layout(location = 4) out vec3 v_normal_cs;

const uint FLAG_LIGHTING = 1u;
const uint FLAG_DIFFUSE_FROM_VERTEX = 2u;
const uint FLAG_FOG = 16u;

vec4 Unpack_Diffuse(uint c)
{
	float a = float((c >> 24u) & 0xffu) / 255.0;
	float r = float((c >> 16u) & 0xffu) / 255.0;
	float g = float((c >> 8u) & 0xffu) / 255.0;
	float b = float(c & 0xffu) / 255.0;
	return vec4(r, g, b, a);
}

vec3 Evaluate_Lighting(vec3 normal_ws, vec3 world_pos)
{
	vec3 ambient = ubo.scene_ambient.rgb * ubo.material_ambient.rgb;
	vec3 lit = ambient + ubo.material_emissive.rgb;

	for (int i = 0; i < 4; ++i) {
		float light_type = ubo.light_params[i].x;
		if (light_type < 0.5) {
			continue;
		}
		vec3 light_dir;
		float atten = 1.0;
		if (light_type < 1.5) {
			light_dir = -normalize(ubo.light_dir_or_pos[i].xyz);
		} else {
			vec3 to_light = ubo.light_dir_or_pos[i].xyz - world_pos;
			float dist = length(to_light);
			if (dist < 0.0001) {
				continue;
			}
			light_dir = to_light / dist;
			atten = 1.0 / max(ubo.light_params[i].y + dist * 0.01, 0.0001);
		}
		float ndotl = max(dot(normal_ws, light_dir), 0.0);
		lit += ubo.light_diffuse[i].rgb * ndotl * atten;
	}
	return lit;
}

vec3 Evaluate_Specular(vec3 normal_cs)
{
	if (ubo.specular_enable < 0.5) {
		return vec3(0.0);
	}
	vec3 spec = vec3(0.0);
	vec3 view_dir_cs = vec3(0.0, 0.0, 1.0);
	float shininess = max(ubo.material_shininess, 1.0);
	vec3 n = normalize(normal_cs);

	for (int i = 0; i < 4; ++i) {
		float light_type = ubo.light_params[i].x;
		if (light_type < 0.5) {
			continue;
		}
		vec3 light_dir_ws;
		if (light_type < 1.5) {
			light_dir_ws = -normalize(ubo.light_dir_or_pos[i].xyz);
		} else {
			continue;
		}
		vec3 light_dir_cs = normalize(mat3(ubo.view) * light_dir_ws);
		vec3 half_vec = normalize(light_dir_cs + view_dir_cs);
		float ndoth = max(dot(n, half_vec), 0.0);
		spec += ubo.light_diffuse[i].rgb * ubo.material_specular.rgb * pow(ndoth, shininess);
	}
	return spec;
}

vec2 Transform_Tex_UV(vec2 uv, vec4 mat)
{
	return vec2(uv.x * mat.x + mat.z, uv.y * mat.y + mat.w);
}

vec2 Compute_Stage_UV(int stage, vec2 uv0, vec2 uv1, vec3 normal_ws, vec3 world_pos)
{
	float tci = (stage == 0) ? ubo.tex_tci[0] : ubo.tex_tci[1];
	float uv_index = (stage == 0) ? ubo.tex_uv_index[0] : ubo.tex_uv_index[1];
	vec4 mat = (stage == 0) ? ubo.tex_mat[0] : ubo.tex_mat[1];

	vec2 base_uv = (uv_index < 0.5) ? uv0 : uv1;
	if (tci < 0.5) {
		return Transform_Tex_UV(base_uv, mat);
	}

	vec3 normal_cs = normalize(mat3(ubo.view) * normal_ws);

	if (tci < 1.5) {
		return Transform_Tex_UV(normal_cs.xy, mat);
	}

	vec3 view_dir_ws = normalize(world_pos - (ubo.view * vec4(0.0, 0.0, 0.0, 1.0)).xyz);
	vec3 reflect_ws = reflect(-view_dir_ws, normal_ws);
	vec3 reflect_cs = normalize(mat3(ubo.view) * reflect_ws);
	if (tci < 2.5) {
		return Transform_Tex_UV(reflect_cs.xy, mat);
	}

	float m = 2.0 * length(vec3(reflect_cs.x, reflect_cs.y, reflect_cs.z + 1.0));
	vec2 sphere_uv = vec2(reflect_cs.x / m + 0.5, -reflect_cs.y / m + 0.5);
	return Transform_Tex_UV(sphere_uv, mat);
}

void main()
{
	vec4 pos = vec4(in_position, 1.0);
	gl_Position = pos * ubo.world * ubo.view_proj;

	vec4 vertex_color;
	if (HAS_DIFFUSE != 0u) {
		vertex_color = Unpack_Diffuse(in_diffuse);
	} else {
		vertex_color = vec4(1.0);
	}

	uint flags = uint(ubo.flags);
	vec3 world_pos = (pos * ubo.world).xyz;
	mat3 normal_matrix = mat3(ubo.world);
	vec3 normal_ws;
	if (HAS_NORMAL != 0u) {
		normal_ws = normalize(normal_matrix * in_normal);
	} else {
		normal_ws = normalize(normal_matrix * vec3(0.0, 0.0, 1.0));
	}
	v_normal_cs = normalize(mat3(ubo.view) * normal_ws);

	if ((flags & FLAG_LIGHTING) != 0u) {
		vec3 lighting = Evaluate_Lighting(normal_ws, world_pos);
		if (ubo.specular_enable > 0.5) {
			lighting += Evaluate_Specular(normalize(v_normal_cs));
		}
		if ((flags & FLAG_DIFFUSE_FROM_VERTEX) != 0u) {
			v_color = vec4(vertex_color.rgb * lighting, vertex_color.a);
		} else {
			v_color = vec4(ubo.material_diffuse.rgb * lighting, ubo.material_diffuse.a * vertex_color.a);
		}
	} else {
		v_color = vertex_color;
	}

	vec2 uv1 = in_uv;
	vec2 uv2 = (TEX_LAYER_COUNT >= 2u) ? in_uv2 : in_uv;
	v_uv = Compute_Stage_UV(0, uv1, uv2, normal_ws, world_pos);
	v_uv2 = Compute_Stage_UV(1, uv1, uv2, normal_ws, world_pos);

	v_fog_factor = 1.0;
	if ((flags & FLAG_FOG) != 0u && ubo.fog_end > ubo.fog_start) {
		vec4 view_pos = pos * ubo.world * ubo.view;
		float depth = abs(view_pos.z);
		v_fog_factor = clamp((ubo.fog_end - depth) / (ubo.fog_end - ubo.fog_start), 0.0, 1.0);
	}
}
