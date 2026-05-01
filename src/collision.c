
#include "cmath.h"
#include <collision.h>
#include <vulkan/vulkan_core.h>

vec3 collision_closest_point_on_segment(vec3 a, vec3 b, vec3 p) {
    vec3 ab = math_vec3_subtract(b, a);
    vec3 ap = math_vec3_subtract(p, a);

    f32 len_sq = math_vec3_len_sqrt(ab);
    if (len_sq < 1e-10f)
        return a;

    f32 t = math_vec3_dot(ap, ab) / len_sq;
    t = math_clamp(t, 0.0f, 1.0f);

    return math_vec3_add(a, math_vec3_scale(ab, t));
}

bool collision_ray_capsule_intersect(vec3 ro, vec3 rd, vec3 seg_a, vec3 seg_b,
                                     f32 radius, f32 *out) {
    vec3 ab = math_vec3_subtract(seg_b, seg_a);
    vec3 ao = math_vec3_subtract(ro, seg_a);

    f32 ab_len_sq = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
    f32 rd_dot_ab = rd.x * ab.x + rd.y * ab.y + rd.z * ab.z;
    f32 ao_dot_ab = ao.x * ab.x + ao.y * ab.y + ao.z * ab.z;

    vec3 rd_perp = {
        rd.x - rd_dot_ab * ab.x / ab_len_sq,
        rd.y - rd_dot_ab * ab.y / ab_len_sq,
        rd.z - rd_dot_ab * ab.z / ab_len_sq,
    };
    vec3 ao_perp = {
        ao.x - ao_dot_ab * ab.x / ab_len_sq,
        ao.y - ao_dot_ab * ab.y / ab_len_sq,
        ao.z - ao_dot_ab * ab.z / ab_len_sq,
    };

    f32 a =
        rd_perp.x * rd_perp.x + rd_perp.y * rd_perp.y + rd_perp.z * rd_perp.z;
    f32 b = 2.0f * (ao_perp.x * rd_perp.x + ao_perp.y * rd_perp.y +
                    ao_perp.z * rd_perp.z);
    f32 c = ao_perp.x * ao_perp.x + ao_perp.y * ao_perp.y +
            ao_perp.z * ao_perp.z - radius * radius;

    f32 best_t = 1e9f;
    bool hit = false;

    if (fabsf(a) > 1e-10f) {
        f32 disc = b * b - 4.0f * a * c;
        if (disc >= 0.0f) {
            f32 sq = sqrtf(disc);
            f32 t0 = (-b - sq) / (2.0f * a);
            f32 t1 = (-b + sq) / (2.0f * a);

            for (int i = 0; i < 2; i++) {
                f32 t = (i == 0) ? t0 : t1;
                if (t < 0.0f)
                    continue;

                vec3 hit_pt = math_vec3_add(ro, math_vec3_scale(rd, t));
                vec3 hit_ao = math_vec3_subtract(hit_pt, seg_a);
                f32 proj = hit_ao.x * ab.x + hit_ao.y * ab.y + hit_ao.z * ab.z;

                if (proj >= 0.0f && proj <= ab_len_sq) {
                    if (t < best_t) {
                        best_t = t;
                        hit = true;
                    }
                }
            }
        }
    }

    vec3 caps[2] = {seg_a, seg_b};
    for (int i = 0; i < 2; i++) {
        vec3 oc = math_vec3_subtract(ro, caps[i]);
        f32 b2 = oc.x * rd.x + oc.y * rd.y + oc.z * rd.z;
        f32 c2 = oc.x * oc.x + oc.y * oc.y + oc.z * oc.z - radius * radius;
        f32 disc = b2 * b2 - c2;
        if (disc < 0.0f)
            continue;
        f32 t = -b2 - sqrtf(disc);
        if (t < 0.0f)
            t = -b2 + sqrtf(disc);
        if (t < 0.0f)
            continue;
        if (t < best_t) {
            best_t = t;
            hit = true;
        }
    }

    if (hit)
        *out = best_t;
    return hit;
}

bool collision_ray_aabb_intersect(vec3 ro, vec3 rd, struct aabb box,
                                  float *out) {
    f32 tmin = -1e9f, tmax = 1e9f;

    // X slab
    if (fabsf(rd.x) > 1e-6f) {
        f32 t1 = (box.min.x - ro.x) / rd.x;
        f32 t2 = (box.max.x - ro.x) / rd.x;
        if (t1 > t2) {
            f32 tmp = t1;
            t1 = t2;
            t2 = tmp;
        }
        tmin = t1 > tmin ? t1 : tmin;
        tmax = t2 < tmax ? t2 : tmax;
    }

    // Y slab
    if (fabsf(rd.y) > 1e-6f) {
        f32 t1 = (box.min.y - ro.y) / rd.y;
        f32 t2 = (box.max.y - ro.y) / rd.y;
        if (t1 > t2) {
            f32 tmp = t1;
            t1 = t2;
            t2 = tmp;
        }
        tmin = t1 > tmin ? t1 : tmin;
        tmax = t2 < tmax ? t2 : tmax;
    }

    // Z slab
    if (fabsf(rd.z) > 1e-6f) {
        f32 t1 = (box.min.z - ro.z) / rd.z;
        f32 t2 = (box.max.z - ro.z) / rd.z;
        if (t1 > t2) {
            f32 tmp = t1;
            t1 = t2;
            t2 = tmp;
        }
        tmin = t1 > tmin ? t1 : tmin;
        tmax = t2 < tmax ? t2 : tmax;
    }

    if (tmax < tmin || tmax < 0.0f)
        return false; // miss

    *out = tmin > 0.0f ? tmin : tmax;
    return true;
}

vec3 collision_screen_to_ray(vec2 mouse, u32 screen_width, u32 screen_height,
                             matrix invers_view, matrix inverse_proj) {
    // ndc
    vec2 ndc = {
        .x = (2.0f * mouse.x) / screen_width - 1.0f,
        .y = (2.0f * mouse.y) / screen_height - 1.0f,
    };

    f32 *p = inverse_proj.m;
    vec4 view_dir = {
        .x = p[0] * ndc.x + p[4] * ndc.y + p[8] * (-1.0f) + p[12],
        .y = p[1] * ndc.x + p[5] * ndc.y + p[9] * (-1.0f) + p[13],
        .z = p[2] * ndc.x + p[6] * ndc.y + p[10] * (-1.0f) + p[14],
        .w = p[3] * ndc.x + p[7] * ndc.y + p[11] * (-1.0f) + p[15],
    };

    if (fabsf(view_dir.w) > 1e-6f) {
        view_dir.x /= view_dir.w;
        view_dir.y /= view_dir.w;
        view_dir.z /= view_dir.w;
    }

    f32 *v = invers_view.m;
    vec3 world = {
        v[0] * view_dir.x + v[4] * view_dir.y + v[8] * view_dir.z,
        v[1] * view_dir.x + v[5] * view_dir.y + v[9] * view_dir.z,
        v[2] * view_dir.x + v[6] * view_dir.y + v[10] * view_dir.z,
    };

    return math_vec3_norm(world);
}
