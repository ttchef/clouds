
#version 460

#extension GL_GOOGLE_include_directive : require

#include "../shader_shared.h"

layout (set = 0, binding = GLOBAL_DESC_MATRIX_BINDING) uniform matrix_ubo {
    mat4 proj;
    mat4 view;
    mat4 proj_view;
} u_matrix;

layout (push_constant) uniform Push {
    mat4 model;
    vec4 color;
} pc;

vec3 cube_verts[8] = vec3[](
    vec3(-1,-1,-1), // 0
    vec3( 1,-1,-1), // 1
    vec3( 1, 1,-1), // 2
    vec3(-1, 1,-1), // 3
    vec3(-1,-1, 1), // 4
    vec3( 1,-1, 1), // 5
    vec3( 1, 1, 1), // 6
    vec3(-1, 1, 1)  // 7
);

int edge_indices[24] = int[](
    0,1, 1,2, 2,3, 3,0,
    4,5, 5,6, 6,7, 7,4,
    0,4, 1,5, 2,6, 3,7
);

void main() {
    int idx = edge_indices[gl_VertexIndex];
    vec3 pos = cube_verts[idx];
    
    gl_Position = u_matrix.proj_view * pc.model * vec4(pos, 1.0);
}

