
#ifndef LIGHT_H
#define LIGHT_H

#include <cmath.h>
#include <shader_shared.h>
#include <types.h>

#include <vk/buffer.h>
#include <vk/image.h>
#include <vk/pipeline.h>

struct renderer;

#define NO_LIGHT -1

typedef i32 light_id;

// cpu definition of light
struct dir_light {
    vec3 direction;
    vec3 color;

    matrix transform;
    u32 shadow_index;
    struct vk_image map;
    bool valid;
};

// cpu definition of light
struct point_light {
    vec3 pos;
    vec3 color;

    f32 constant;
    f32 linear;
    f32 quadratic;

    matrix transform;
    u32 shadow_index;
    struct vk_image map;
    bool valid;
};

// cpu definition of light
struct spot_light {
    vec3 pos;
    vec3 direction;
    vec3 color;

    f32 cutt_off;
    f32 outer_cutt_off;

    f32 constant;
    f32 linear;
    f32 quadratic;

    matrix transform;
    u32 shadow_index;
    struct vk_image map;
    bool valid;
};

// needs to be with valid alignement
// for gpu uniform buffers
struct __attribute__((aligned(16))) gpu_dir_light {
    vec4 direction;
    vec4 color;

    u32 shadow_index;
    u32 pad0;
    u32 pad1;
    u32 pad2;

    matrix transform;
};

struct __attribute__((aligned(16))) gpu_point_light {
    vec4 pos;
    vec4 color;

    // where x is constant, y is linear and z is qudratic
    vec4 attenuation;

    u32 shadow_index;
    u32 pad0;
    u32 pad1;
    u32 pad2;

    matrix transform;
};

struct __attribute__((aligned(16))) gpu_spot_light {
    vec4 pos;
    vec4 direction;
    vec4 color;

    // where x is cutt_off and y is outer_cutt_off
    vec4 cut_offs;
    // where x is constant, y is linear and z is qudratic
    vec4 attenuation;

    u32 shadow_index;
    u32 pad0;
    u32 pad1;
    u32 pad2;

    matrix transform;
};

// watch out for alignement
struct light_buffer {
    struct gpu_dir_light directional[MAX_DIRECTIONAL_LIGHTS];
    struct gpu_point_light point[MAX_POINT_LIGHTS];
    struct gpu_spot_light spot[MAX_SPOT_LIGHTS];

    u32 directional_count;
    u32 point_count;
    u32 spot_count;
    u32 padding;
};

// works the same as the texture manager
struct light_manager {
    struct vk_buffer buffers[FRAMES_IN_FLIGHT];

    struct dir_light directional[MAX_DIRECTIONAL_LIGHTS];
    struct point_light point[MAX_POINT_LIGHTS];
    struct spot_light spot[MAX_SPOT_LIGHTS];

    u32 directional_counter;
    u32 spot_counter;

    vk_pipeline_id shadow_pip;

    struct light_buffer light_buffer;
    bool render_lights;
};

bool light_manager_create(struct renderer *r, struct light_manager *manager);

void light_manager_destroy(struct renderer *r, struct light_manager *manager);

light_id light_dir_create(struct renderer *r, vec3 direction, vec3 color);

light_id light_point_create(struct renderer *r, vec3 pos, vec3 color,
                            f32 distance);

light_id light_spot_create(struct renderer *r, vec3 pos, vec3 direction,
                           vec3 color, f32 distance, f32 cutt_of,
                           f32 outer_cutt_of);

void light_dir_update(struct renderer *r, light_id id, vec3 direction,
                      vec3 color);

void light_point_update(struct renderer *r, light_id id, vec3 pos, vec3 color,
                        f32 distance);

void light_spot_update(struct renderer *r, light_id id, vec3 pos,
                       vec3 direction, vec3 color, f32 distance, f32 cutt_of,
                       f32 outer_cutt_of);

void light_destroy(struct renderer *r, light_id id);

void light_state_set(struct renderer *r, light_id id, bool on);

// upload light data to uniform buffer on gpu
bool light_sync_gpu(struct renderer *r);

void light_set_render(struct renderer *r, bool render);

#endif // LIGHT_H
