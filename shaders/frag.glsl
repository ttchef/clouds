
#version 460

layout (location = 0) in vec4 in_color;
layout (location = 1) in vec2 in_uv;
layout (location = 2) in vec3 in_normal;

layout (location = 0) out vec4 out_color;

void main() {
    vec3 unit_norm = normalize(in_normal);
    vec3 light_dir = normalize(vec3(-0.5, -0.5, 0.0));

    float brightness = max(dot(unit_norm, light_dir), 0.1);
    
    out_color = vec4(in_color.xyz * brightness, in_color.w);
}
