
#ifndef COLLISION_H
#define COLLISION_H

#include <cmath.h>

struct aabb {
    vec3 min;
    vec3 max;
};

static bool collision_overlap_aabb(struct aabb a, struct aabb b) {
    return (a.min.x < b.max.x && a.max.x > b.min.x && a.min.y < b.max.y &&
            a.max.y > b.min.y && a.min.z < b.max.z && a.max.z > b.min.z);
}

static struct aabb collision_get_aabb(vec3 pos, vec3 scale) {
    vec3 half = math_vec3_scale(scale, 0.5f);
    return (struct aabb){
        .min = math_vec3_subtract(pos, half),
        .max = math_vec3_add(pos, half),
    };
}

bool collision_ray_aabb_intersect(vec3 ro, vec3 rd, struct aabb box,
                                  float *out);

vec3 collision_screen_to_ray(vec2 mouse, u32 screen_width, u32 screen_height,
                             matrix invers_view, matrix inverse_proj);

#endif // COLLISION_H
