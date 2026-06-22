#version 450

layout(location=0) in vec2 in_pos;
layout(location=1) in vec2 in_uv;
layout(location=2) in uint in_color;

layout(location=0) out vec2 v_uv;
layout(location=1) out vec4 v_color;

vec4 Unpack_Color(uint c)
{
	float a = float((c >> 24u) & 0xffu) / 255.0;
	float r = float((c >> 16u) & 0xffu) / 255.0;
	float g = float((c >>  8u) & 0xffu) / 255.0;
	float b = float((c       ) & 0xffu) / 255.0;
	return vec4(r, g, b, a);
}

void main()
{
	gl_Position = vec4(in_pos, 0.0, 1.0);
	v_uv = in_uv;
	v_color = Unpack_Color(in_color);
}
