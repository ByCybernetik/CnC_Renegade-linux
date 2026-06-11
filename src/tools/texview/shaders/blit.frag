#version 450

layout(location = 0) in vec2 v_uv;

layout(set = 0, binding = 0) uniform sampler2D tex;

layout(push_constant) uniform Push {
	vec2 tex_size;
	float show_alpha_bg;
} pc;

layout(location = 0) out vec4 out_color;

void main()
{
	vec4 texel = texture(tex, v_uv);

	if (pc.show_alpha_bg > 0.5 && texel.a < 0.999) {
		vec2 grid = v_uv * pc.tex_size / 8.0;
		float checker = mod(floor(grid.x) + floor(grid.y), 2.0);
		vec3 bg = mix(vec3(0.18), vec3(0.30), checker);
		texel.rgb = mix(bg, texel.rgb, texel.a);
		texel.a = 1.0;
	}

	out_color = texel;
}
