
#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : enable

#include "../shader_shared.h"
#include "include/light.glsl"

layout (location = 0) in vec2 in_uv;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_world_pos;

layout (set = 0, binding = GLOBAL_DESC_TEXTURE_BINDING) uniform sampler2D in_textures[MAX_TEXTURES];

layout (set = 0, binding = GLOBAL_DESC_LIGHT_BINDING) uniform lights {
    DirLight directional[MAX_DIRECTIONAL_LIGHTS];
    PointLight point[MAX_POINT_LIGHTS];
    SpotLight spot[MAX_SPOT_LIGHTS];

    uint directional_count;
    uint point_count;
    uint spot_count;

    uint padding;
} u_lights;

layout (set = 0, binding = GLOBAL_DESC_SHADOW_DIRECTIONAL_BINDING) uniform sampler2D u_shadow_directional[MAX_DIRECTIONAL_LIGHTS];
layout (set = 0, binding = GLOBAL_DESC_SHADOW_POINT_BINDING) uniform sampler2D u_shadow_point[MAX_POINT_LIGHTS];
layout (set = 0, binding = GLOBAL_DESC_SHADOW_SPOT_BINDING) uniform sampler2D u_shadow_spot[MAX_SPOT_LIGHTS];

layout (push_constant) uniform Push {
    mat4 model;
    vec4 view_pos;
    uint texture_index;
} pc;

layout (location = 0) out vec4 out_color;

void main() {
    float gamma = 2.2;
    vec3 light_out = vec3(0.0);

    vec3 normal = normalize(in_normal);
    vec3 view_dir = normalize(pc.view_pos.xyz - in_world_pos);
    vec4 tex_sample = texture(in_textures[nonuniformEXT(pc.texture_index)], in_uv);
    vec3 color = pow(tex_sample.xyz, vec3(gamma));

    for (int i = 0; i < u_lights.directional_count; i++) {
        DirLight light = u_lights.directional[i];
        light_out += calc_dir_light(light, normal, in_world_pos, view_dir, color, u_shadow_directional[nonuniformEXT(light.shadow_index)]);
    }

    for (int i = 0; i < u_lights.point_count; i++) {
        light_out += calc_point_light(u_lights.point[i], normal, in_world_pos, view_dir, color);
    }

    for (int i = 0; i < u_lights.spot_count; i++) {
        SpotLight light = u_lights.spot[i];
        light_out += calc_spot_light(light, normal, in_world_pos, view_dir, color, u_shadow_spot[nonuniformEXT(light.shadow_index)]);
    }

    // gamma correction
    light_out = pow(light_out, vec3(1.0 / gamma));
    vec4 light_space = u_lights.spot[0].transform * vec4(in_world_pos, 1.0);
    vec3 coords = light_space.xyz / light_space.w;
    coords.xy = coords.xy * 0.5 + 0.5;
    coords.y = 1.0 - coords.y;

    float damn = texture(u_shadow_spot[nonuniformEXT(0)], coords.xy).r;

    // light_out = vec3(damn);
    out_color = vec4(light_out, tex_sample.w);
}


