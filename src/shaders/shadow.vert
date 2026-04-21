
#version 460

#extension GL_GOOGLE_include_directive : require

#include "../shader_shared.h"

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec2 in_uv;
layout (location = 2) in vec3 in_normal;

layout (set = 0, binding = GLOBAL_DESC_MATRIX_BINDING) uniform matrix_ubo {
    mat4 proj;
    mat4 view;
    mat4 proj_view;
} u_matrix;

layout (push_constant) uniform Push {
    mat4 model;
    mat4 light_space;
} pc;

void main() {
    gl_Position = pc.light_space * pc.model * vec4(in_pos, 1.0);
}
