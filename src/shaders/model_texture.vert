#version 460

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec2 in_uv;
layout (location = 2) in vec3 in_normal;

layout (push_constant) uniform Push {
    mat4 m;
    uint texture_index;
} pc;

layout (location = 0) out vec2 out_uv;
layout (location = 1) out vec3 out_normal;

void main() {
    gl_Position = pc.m * vec4(in_pos, 1.0);
    out_uv = in_uv;
    out_normal = in_normal;
}

