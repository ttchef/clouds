#version 460

#extension GL_GOOGLE_include_directive : require

#include "../shader_shared.h"

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec2 in_uv;
layout (location = 2) in vec3 in_normal;

layout (set = 0, binding = GLOBAL_DESC_MATRIX_BINDING) uniform matrix_ubo {
    mat4 proj_view;
    mat4 light_space;
} u_matrix;

layout (push_constant) uniform Push {
    mat4 model;
    vec4 view_pos;
    uint texture_index;
} pc;

layout (location = 0) out vec2 out_uv;
layout (location = 1) out vec3 out_normal;
layout (location = 2) out vec3 out_world_pos;

void main() {
    gl_Position = u_matrix.proj_view * pc.model * vec4(in_pos, 1.0);
    out_uv = in_uv;

    // TODO: change to cpu side calculation
    out_normal = mat3(transpose(inverse(pc.model))) * in_normal;

    vec4 pos = pc.model * vec4(in_pos, 1.0);
    out_world_pos = pos.xyz;
}

