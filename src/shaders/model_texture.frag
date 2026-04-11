
#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : enable

#include "../shader_shared.h"
#include "include/light.glsl"

layout (location = 0) in vec2 in_uv;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_world_pos;

layout (set = 0, binding = GLOBAL_DESC_TEXTURE_BINDING) uniform sampler2D in_textures[];

layout (set = 0, binding = GLOBAL_DESC_LIGHT_BINDING) uniform lights {
    DirLight directional[MAX_DIRECTIONAL_LIGHTS];
    PointLight point[MAX_POINT_LIGHTS];
    SpotLight spot[MAX_SPOT_LIGHTS];

    uint directional_count;
    uint point_count;
    uint spot_count;

    uint padding;
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

    for (int i = 0; i < u_lights.directional_count; i++) {
        light_out += calc_dir_light(u_lights.directional[i], normal, view_dir, tex_sample.xyz);
    }

    for (int i = 0; i < u_lights.point_count; i++) {
        light_out = calc_point_light(u_lights.point[i], normal, in_world_pos, view_dir, tex_sample.xyz);
    }

    out_color = vec4(light_out, tex_sample.w);
}


