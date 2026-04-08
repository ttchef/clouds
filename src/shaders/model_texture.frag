
#version 460

layout (location = 0) in vec2 in_uv;
layout (location = 1) in vec3 in_normal;

layout (set = 0, binding = 0) uniform sampler2D in_texture;

layout (location = 0) out vec4 out_color;

void main() {
    vec3 unit_norm = normalize(in_normal);
    vec3 light_dir = normalize(vec3(-0.5, -0.5, 0.0));

    float brightness = max(dot(unit_norm, light_dir), 0.1);

    vec4 tex_sample = texture(in_texture, in_uv);
    
    out_color = vec4(tex_sample.xyz * brightness, tex_sample.w);
}

