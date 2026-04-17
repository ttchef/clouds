
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
    DirLight directional[MAX_DIRECTIONAL_LIGHTS];
    PointLight point[MAX_POINT_LIGHTS];
    SpotLight spot[MAX_SPOT_LIGHTS];

    uint directional_count;
    uint point_count;
    uint spot_count;

    uint padding;
} u_lights;

layout (set = 0, binding = GLOBAL_DESC_SHADOW_DIRECTIONAL_BINDING) uniform sampler2D u_shadow_directional[];
layout (set = 0, binding = GLOBAL_DESC_SHADOW_POINT_BINDING) uniform sampler2D u_shadow_point[];
layout (set = 0, binding = GLOBAL_DESC_SHADOW_SPOT_BINDING) uniform sampler2D u_shadow_spot[];

layout (push_constant) uniform Push {
    mat4 model;
    vec4 view_pos;
    vec4 color;
} pc;

layout (location = 0) out vec4 out_color;

void main() {
    float gamma = 2.2;
    vec3 light_out = vec3(0.0);

    vec3 normal = normalize(in_normal);
    vec3 view_dir = normalize(pc.view_pos.xyz - in_world_pos);
    vec3 color = pow(pc.color.xyz, vec3(gamma));

    for (int i = 0; i < u_lights.directional_count; i++) {
        DirLight light = u_lights.directional[i];
        light_out += calc_dir_light(light, normal, in_world_pos, view_dir, color, u_shadow_directional[nonuniformEXT(light.shadow_index)]);
    }

    for (int i = 0; i < u_lights.point_count; i++) {
        light_out += calc_point_light(u_lights.point[i], normal, in_world_pos, view_dir, color);
    }

    for (int i = 0; i < u_lights.spot_count; i++) {
        light_out += calc_spot_light(u_lights.spot[i], normal, in_world_pos, view_dir, color);
    }

    // gamma correction
    light_out = pow(light_out, vec3(1.0 / gamma));
    
    out_color = vec4(light_out, pc.color.w);
}
