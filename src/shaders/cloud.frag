
#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : enable

#include "../shader_shared.h"
#include "include/light.glsl"

layout (location = 0) in vec2 in_uv;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_world_pos;
layout (location = 3) in mat4 in_inverse_model;

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

#define LIGHT_STEPS 6
#define LIGHT_STEP_SIZE 0.1
#define EXTINCTION 3.3
#define SCATTERING 2.3
#define HG_G 0.6 // henyes greenstein anisotropy (0 isotropic - 1 ansiotropy)
#define DENSITY_THRESHOLD 0.05
#define DUAL_LOP_COEFF 0.7

vec3 cloud_scale = vec3(
    length(pc.model[0].xyz),
    length(pc.model[1].xyz),
    length(pc.model[2].xyz)
);

vec2 intersect_box(vec3 ray_origin, vec3 ray_dir, vec3 box_min, vec3 box_max) {
    vec3 t0 = (box_min - ray_origin) / ray_dir;
    vec3 t1 = (box_max - ray_origin) / ray_dir;

    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);

    float entry = max(max(tmin.x, tmin.y), tmin.z);
    float exit = min(min(tmax.x, tmax.y), tmax.z);

    return vec2(entry, exit);
}
float remap(float v, float lo, float hi, float newLo, float newHi) {
    return newLo + (v - lo) / (hi - lo) * (newHi - newLo);
}

float saturate(float x) { return clamp(x, 0.0, 1.0); }

float sample_density(vec3 p, vec3 scale) {
    float avg_scale = (scale.x + scale.y + scale.z) / 3.0;
    vec3 uvw = (p + 0.5) * (scale / avg_scale);

    float h = p.y + 0.5;

    float grad_bottom = saturate(remap(h, 0.0, 0.2, 0.0, 1.0));
    float grad_top    = saturate(remap(h, 0.35, 1.0, 1.0, 0.0));
    float height_grad = grad_bottom * grad_top;

    if (height_grad < 0.001) {
        return 0.0;
    }

    vec3 wind0 = vec3(0.05, 0.0, 0.02);
    vec3 wind1 = vec3(-0.02, 0.0, 0.04);

    vec4 noise_low1 = texture(u_noise, uvw * 0.25 + wind0 * pc.time * 2.0);
    vec4 noise_low2 = texture(u_noise, uvw * 0.35 + wind1 * pc.time * 1.8);
    vec4 noise_med1 = texture(u_noise, uvw * 0.6 + wind0 * pc.time * 2.5);
    vec4 noise_med2 = texture(u_noise, uvw * 0.8 + wind1 * pc.time * 2.2);
    vec4 noise_high1 = texture(u_noise, uvw * 1.5 + wind0 * pc.time * 4.0);
    vec4 noise_high2 = texture(u_noise, uvw * 2.0 + wind1 * pc.time * 3.5);

    float base_low = mix(noise_low1.r, noise_low1.g, 0.5);
    float base_low2 = mix(noise_low2.r, noise_low2.g, 0.5);
    float base = mix(base_low, base_low2, 0.5);

    float cellular = mix(noise_low1.a, noise_low2.a, 0.5) * 0.3;
    float cellular_med = mix(noise_med1.a, noise_med2.a, 0.5) * 0.15;
    base += cellular + cellular_med;

    float shaped = base * height_grad;

    float coverage = 0.4;
    shaped = saturate(remap(shaped, coverage, 1.0, 0.0, 1.0));

    if (shaped < 0.001) return 0.0;

    float detail_low1 = noise_low1.b;
    float detail_low2 = noise_low2.b;
    float detail_med1 = noise_med1.b;
    float detail_med2 = noise_med2.b;
    float detail_high1 = noise_high1.b;
    float detail_high2 = noise_high2.b;

    float detail = mix(detail_low1, detail_low2, 0.5);
    detail = mix(detail, mix(detail_med1, detail_med2, 0.5), 0.6);
    detail = mix(detail, mix(detail_high1, detail_high2, 0.5), 0.3);

    float detail_worley = mix(noise_med1.a, noise_med2.a, 0.5) * 0.2;
    detail_worley += mix(noise_high1.a, noise_high2.a, 0.5) * 0.1;
    detail += detail_worley;

    float detail_mod = mix(detail, 1.0 - detail * 0.6, saturate(h * 3.0));

    float d = saturate(remap(shaped, detail_mod * 0.1, 0.82, 0.0, 1.0));

    d = pow(d, 0.87);

    return d;
}

float light_transmittance(vec3 p, vec3 light_dir, vec3 scale) {
    float shadow = 0.0;
    vec3 lp = p;

    for (int i = 0; i < LIGHT_STEPS; i++) {
        lp += light_dir * LIGHT_STEP_SIZE;

        if (any(lessThan(lp, vec3(-0.5))) || any(greaterThan(lp, vec3(0.5)))) {
            break;
        }
        shadow += sample_density(lp, scale) * LIGHT_STEP_SIZE;
        if (shadow > 2.0) {
            return 0.0;
        }
    }
    return exp(-shadow * EXTINCTION);
}

float henyey_greenstein(float cos_theta, float g) {
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * 3.14159 * pow(1.0 + g2 - 2.0 * g * cos_theta, 1.5));    
}

float dual_lop_henyey_greenstein(float cos_theta, float g) {
    float forward = henyey_greenstein(cos_theta, g);
    float backward = henyey_greenstein(cos_theta, -g);

    return mix(backward, forward, DUAL_LOP_COEFF); 
}

vec3 multi_scatter(float cos_theta, vec3 sun_color, float light) {
    vec3 result = vec3(0.0);
    float scatter_ms = SCATTERING;
    float g_ms = HG_G;

    for (int i = 0; i < 2; i++) {
        float phase = dual_lop_henyey_greenstein(cos_theta, g_ms);
        float light_ms = pow(light, pow(0.5, float(i)));
        result += scatter_ms * phase * sun_color * light_ms * pow(0.5, float(i));

        scatter_ms *= 0.5;
        g_ms *= 0.5;
    }

    return result;
}

float jitter(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float beer_lambert(float d, float step_size) {
    return exp(-d * EXTINCTION * step_size);
}

void main() {
    float gamma = 2.2;

    vec3 normal = normalize(in_normal);
    vec3 color = pow(pc.color.xyz, vec3(gamma));
    // vec3 color = pow(vec3(1.0, 0.0, 0.0), vec3(gamma));

    vec3 sun_color = vec3(1.0, 0.95, 0.8);
    vec3 ambient_color = vec3(0.4, 0.55, 0.8);

    // world space
    vec3 ray_origin_ws = pc.cam_pos.xyz;
    vec3 ray_dir_ws = normalize(in_world_pos - ray_origin_ws);

    mat4 inverse_model = in_inverse_model;

    // object space
    vec3 ray_origin = (inverse_model * vec4(ray_origin_ws, 1.0)).xyz;
    vec3 ray_dir = normalize((inverse_model * vec4(ray_dir_ws, 0.0)).xyz);

    vec3 sun_dir_ws = normalize(vec3(-0.5, -0.5, 0.0));
    vec3 sun_dir = normalize((inverse_model * vec4(sun_dir_ws, 0.0)).xyz);

    vec3 box_min = vec3(-0.5);
    vec3 box_max = vec3(0.5);

    vec2 hit = intersect_box(ray_origin, ray_dir, box_min, box_max);

    if (hit.x > hit.y) {
        discard;
    }

    float step_size = 0.02;
    
    float jit = jitter(gl_FragCoord.xy) * step_size;
    float t = max(hit.x, 0.0) + jit;
    float end = hit.y;

    vec3 col = vec3(0.0);
    float transmittance = 1.0;

    float cos_theta = dot(ray_dir, sun_dir);
    float phase = henyey_greenstein(cos_theta, HG_G);

    // for optimisation
    bool in_cloud = false;
    float big_step = step_size * 5.0;

    while (t < end) {
        vec3 p = ray_origin + t * ray_dir;

        float d = sample_density(p, cloud_scale);
        d = max(0.0, d - DENSITY_THRESHOLD);

        if (!in_cloud && d < 0.001) {
            t += big_step;
            continue;
        }

        in_cloud = true;
        t += step_size;

        // Beer lamber law
        float absorb = beer_lambert(d, step_size);

        float light = light_transmittance(p, sun_dir, cloud_scale);
        vec3 lighting = multi_scatter(cos_theta, sun_color, light) + 0.15 * ambient_color;
        
        vec3 scattering = d * step_size * transmittance * lighting;

        col += scattering;
        // col += d * SCATTERING * step_size * transmittance * pc.color.xyz;
        transmittance *= absorb;

        if (transmittance < 0.01) {
            break;
        }

        if (d < 0.001) in_cloud = false;
    }

    out_color = vec4(col, 1.0 - transmittance);
}



