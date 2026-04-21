
#version 460

#extension GL_GOOGLE_include_directive : require

#include "../shader_shared.h"

layout (location = 0) in vec3 in_dir;

layout (set = 0, binding = GLOBAL_DESC_SKYBOX_BINDING) uniform samplerCube skybox;

layout (location = 0) out vec4 out_color;

void main() {
    out_color = texture(skybox, normalize(in_dir));
}
