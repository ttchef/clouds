
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

layout (set = 0, binding = GLOBAL_DESC_SKYBOX_BINDING) uniform samplerCube u_skybox;
layout (set = 0, binding = GLOBAL_DESC_NOISE_3D_BINDING) uniform sampler3D u_noise;

layout (push_constant) uniform Push {
    mat4 model;
    vec4 cam_pos;
    vec4 color;
    float time;
} pc;

layout (location = 0) out vec4 out_color;

vec2 intersect_box(vec3 ray_origin, vec3 ray_dir, vec3 box_min, vec3 box_max) {
    vec3 t0 = (box_min - ray_origin) / ray_dir;
    vec3 t1 = (box_max - ray_origin) / ray_dir;

    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);

    float entry = max(max(tmin.x, tmin.y), tmin.z);
    float exit = min(min(tmax.x, tmax.y), tmax.z);

    return vec2(entry, exit);
}

float sample_density(vec3 p) {
    vec3 uvw = (p + 1.0) * 0.5;

    vec3 wind0 = vec3(0.05, 0.0, 0.02);
    vec3 wind1 = vec3(-0.02, 0.0, 0.04);
        
    float d = 0.0;

    d += 0.6 * texture(u_noise, uvw * 0.5 + wind0 * pc.time * 2.0).r;
    d += 0.3 * texture(u_noise, uvw * 1.0 + wind1 * pc.time * 1.5).r;
    d += 0.1 * texture(u_noise, uvw * 2.0 - wind0 * pc.time * 5.5).r;
    
    return d;
}

void main() {
    float gamma = 2.2;

    vec3 normal = normalize(in_normal);
    vec3 color = pow(pc.color.xyz, vec3(gamma));
    // vec3 color = pow(vec3(1.0, 0.0, 0.0), vec3(gamma));

    // world space
    vec3 ray_origin_ws = pc.cam_pos.xyz;
    vec3 ray_dir_ws = normalize(in_world_pos - ray_origin_ws);

    mat4 inverse_model = inverse(pc.model);

    // object space
    vec3 ray_origin = (inverse_model * vec4(ray_origin_ws, 1.0)).xyz;
    vec3 ray_dir = normalize((inverse_model * vec4(ray_dir_ws, 0.0)).xyz);

    vec3 box_min = vec3(-1.0);
    vec3 box_max = vec3(1.0);

    vec2 hit = intersect_box(ray_origin, ray_dir, box_min, box_max);

    if (hit.x > hit.y) {
        discard;
    }

    float t = max(hit.x, 0.0);
    float end = hit.y;

    float step_size = 0.005;

    vec3 col = vec3(0.0);
    float transmittance = 1.0;

    for (; t < end; t += step_size) {
        vec3 p = ray_origin + t * ray_dir;

        // TODO: noise density
        float d = sample_density(p);

        // Beer lamber law
        float absorb = exp(-d * step_size);

        transmittance *= absorb;

        col += transmittance * d * color * step_size;

        if (transmittance < 0.01) {
            break;
        }
    }

    // gamma correction
    color = pow(color, vec3(1.0 / gamma));

    // out_color = vec4(color, 1.0);

    out_color = vec4(col, 1.0 - transmittance);
}



