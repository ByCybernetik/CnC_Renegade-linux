#version 450

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec2 v_uv;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D u_tex;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    uint use_texture;
    uint alpha_test;
} pc;

void main() {
    vec3 light_dir = normalize(vec3(0.3, 0.8, 0.5));
    vec3 normal = normalize(v_normal);
    float diffuse = max(dot(normal, light_dir), 0.0);
    float ambient = 0.35;

    vec3 albedo = vec3(0.8);
    float alpha = 1.0;
    if (pc.use_texture != 0u) {
        vec2 uv = vec2(v_uv.x, 1.0 - v_uv.y);
        vec4 tex = texture(u_tex, uv);
        albedo = tex.rgb;
        alpha = tex.a;
    }

    /* D3D8 default alpha reference: 0x60 (see ShaderClass::Apply). */
    if (pc.alpha_test != 0u && alpha < (96.0 / 255.0)) {
        discard;
    }

    vec3 color = albedo * (ambient + diffuse * 0.65);
    out_color = vec4(color, alpha);
}
