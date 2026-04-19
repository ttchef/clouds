
#extension GL_GOOGLE_include_directive : require

#include "../../shader_shared.h"

float calc_shadow(sampler2D shadowMap, vec4 light_space_pos, vec3 normal, vec3 light_dir) {
    vec3 proj = light_space_pos.xyz / light_space_pos.w;

    // vec2 uv = vec2(proj.x * 0.5 + 0.5, proj.y * -0.5 + 0.5);
    vec2 uv = proj.xy * 0.5 + 0.5;

    if (uv.x < 0.0 || uv.x > 1.0 ||
        uv.y < 0.0 || uv.y > 1.0 ||
        proj.z < 0.0 || proj.z > 1.0)
        return 0.0;

    float closest_depth = texture(shadowMap, uv).r;
    float current_depth = proj.z;

    float bias = max(0.005 * (1.0 - dot(normal, light_dir)), 0.0005);

    return current_depth - bias > closest_depth ? 1.0 : 0.0;
}
