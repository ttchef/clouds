
#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : enable

#include "include/light.glsl"

layout (location = 0) in vec2 in_uv;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_world_pos;

layout (set = 0, binding = 0) uniform sampler2D in_textures[];
layout (set = 0, binding = 1) uniform lights {
    Light lights[MAX_LIGHTS];
    uint count;
} u_lights;

layout (push_constant) uniform Push {
    mat4 model;
    vec4 view_pos;
    uint texture_index;
} pc;

layout (location = 0) out vec4 out_color;

void main() {
    vec3 light_out = vec3(0.0);

    vec3 normal = normalize(in_normal);
    vec3 view_dir = normalize(pc.view_pos.xyz - in_world_pos);
    vec4 tex_sample = texture(in_textures[nonuniformEXT(pc.texture_index)], in_uv);

    for (int i = 0; i < u_lights.count; i++) {
        light_out += calc_dir_light(u_lights.lights[i], normal, view_dir, tex_sample.xyz);
    }

    out_color = vec4(light_out, tex_sample.w);
}


