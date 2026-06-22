#version 450

layout(push_constant) uniform PushConstants {
	vec4 modulate_color;
	float texture_enabled;
} pc;

layout(set=0, binding=0) uniform sampler2D tex;

layout(location=0) in vec2 v_uv;
layout(location=1) in vec4 v_color;

layout(location=0) out vec4 out_color;

void main()
{
	vec4 texel = (pc.texture_enabled > 0.5) ? texture(tex, v_uv) : vec4(1.0);
	out_color = texel * v_color * pc.modulate_color;
}
