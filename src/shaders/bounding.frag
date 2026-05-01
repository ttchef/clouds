
#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : enable

#include "../shader_shared.h"
#include "include/light.glsl"

// has access to them but doesnt use rn
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

layout (set = 0, binding = GLOBAL_DESC_SKYBOX_BINDING) uniform samplerCube skybox;

layout (push_constant) uniform Push {
    mat4 model;
    vec4 color;
} pc;

layout (location = 0) out vec4 out_color;

void main() {
    float gamma = 2.2;
    vec3 color = pow(pc.color.xyz, vec3(gamma));

    // gamma correction
    color = pow(color, vec3(1.0 / gamma));
    
    out_color = vec4(color, pc.color.w);
}


