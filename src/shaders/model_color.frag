
#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : enable

#include "../shader_shared.h"
#include "include/light.glsl"

layout (location = 0) in vec2 in_uv;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_world_pos;

// has access to them but doesnt use rn
layout (set = 0, binding = GLOBAL_DESC_TEXTURE_BINDING) uniform sampler2D in_textures[];
layout (set = 0, binding = GLOBAL_DESC_LIGHT_BINDING) uniform lights {
    Light lights[MAX_LIGHTS];
    uint count;
} u_lights;

layout (push_constant) uniform Push {
    mat4 model;
    vec4 view_pos;
    vec4 color;
} pc;

layout (location = 0) out vec4 out_color;

void main() {
    vec3 light_out = vec3(0.0);

    vec3 normal = normalize(in_normal);
    vec3 view_dir = normalize(pc.view_pos.xyz - in_world_pos);

    for (int i = 0; i < u_lights.count; i++) {
        light_out += calc_dir_light(u_lights.lights[i], normal, view_dir, pc.color.xyz);
    }
    
    out_color = vec4(light_out, pc.color.w);
}
