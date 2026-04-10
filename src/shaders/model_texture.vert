#version 460

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec2 in_uv;
layout (location = 2) in vec3 in_normal;

layout (set = 0, binding = 2) uniform matrix_ubo {
    mat4 proj_view;
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

