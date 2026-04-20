
#version 460

#extension GL_GOOGLE_include_directive : require

#include "../shader_shared.h"

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec2 in_uv;
layout (location = 2) in vec3 in_normal;

layout (location = 0) out vec3 out_dir;

layout (set = 0, binding = GLOBAL_DESC_MATRIX_BINDING) uniform matrix_ubo {
    mat4 proj;
    mat4 view;
    mat4 proj_view;
} u_matrix;

void main() {
    out_dir = in_pos;

    // cancel view translation
    mat4 view_rot = mat4(mat3(u_matrix.view));

    vec4 pos = u_matrix.proj * view_rot * vec4(in_pos, 1.0);
    gl_Position = pos.xyww;
}
