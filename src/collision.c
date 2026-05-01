
#include <collision.h>
#include <vulkan/vulkan_core.h>

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
