
#extension GL_GOOGLE_include_directive : require

#include "../../shader_shared.h"

float calc_shadow(sampler2D shadowMap, vec4 light_space_pos, vec3 normal, vec3 light_dir) {
    vec3 projCoords = light_space_pos.xyz / light_space_pos.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    projCoords.y = 1.0 - projCoords.y;

    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0)
        return 0.0;

    float closest_depth = texture(shadowMap, projCoords.xy).r;
    float current_depth = projCoords.z;

    float bias = max(0.005 * (1.0 - dot(normal, light_dir)), 0.0005);

    return current_depth - bias > closest_depth ? 1.0 : 0.0;
}
