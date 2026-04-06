
#version 460

layout (location = 0) in vec3 in_pos;

layout (push_constant) uniform Push {
    mat4 m;
    vec4 color;
} pc;

layout (location = 0) out vec4 out_color;

void main() {
    gl_Position = pc.m * vec4(in_pos, 1.0);
    out_color = pc.color;
}
