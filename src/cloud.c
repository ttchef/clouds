
#include "gizmo.h"
#include <cloud.h>
#include <log.h>
#include <renderer.h>

#include <string.h>

void cloud_manager_init(struct cloud_manager *manager) {
    memset(manager, 0, sizeof(*manager));
    manager->selected_cloud = NO_CLOUD;
}

static void make_gizmo(struct cloud *c) {
    struct gizmo *g = &c->gizmo;
    f32 len = 1.5f; // gizmo arm len

    g->hitboxes[0] = (struct gizmo_hitbox){
        .start = c->pos,
        .end = math_vec3_add(c->pos, (vec3){len, 0.0f, 0.0f}),
        .radius = 0.1f,
        .axis = GIZMO_AXIS_X,
    };

    g->hitboxes[1] = (struct gizmo_hitbox){
        .start = c->pos,
        .end = math_vec3_add(c->pos, (vec3){0.0f, len, 0.0f}),
        .radius = 0.1f,
        .axis = GIZMO_AXIS_Y,
    };

    g->hitboxes[2] = (struct gizmo_hitbox){
        .start = c->pos,
        .end = math_vec3_add(c->pos, (vec3){0.0f, 0.0f, len}),
        .radius = 0.1f,
        .axis = GIZMO_AXIS_Z,
    };

    g->active_axis = GIZMO_AXIS_NONE;
}

cloud_id cloud_create(struct renderer *r, vec3 pos, vec3 scale) {
    struct cloud_manager *m = &r->cloud_manager;

    cloud_id id = NO_CLOUD;

    for (u32 i = 0; i < MAX_CLOUDS; i++) {
        struct cloud *c = &m->clouds[i];

        if (c->valid) {
            continue;
        }

        *c = (struct cloud){
            .pos = pos,
            .scale = scale,
            .valid = true,
        };

        make_gizmo(c);

        id = i;
        break;
    }

    return id;
}

void cloud_update_gizmo(struct renderer *r, cloud_id cloud) {
    if (cloud < 0 || cloud >= MAX_CLOUDS) {
        LOGM(WARN, "cloud id is invalid: %d", cloud);
        return;
    }

    struct cloud_manager *m = &r->cloud_manager;
    struct cloud *c = &m->clouds[cloud];
    make_gizmo(c);
}

struct cloud *cloud_get(struct renderer *r, cloud_id cloud) {
    if (cloud < 0 || cloud >= MAX_CLOUDS) {
        LOGM(WARN, "cloud id is invalid: %d", cloud);
        return NULL;
    }

    struct cloud_manager *m = &r->cloud_manager;

    return &m->clouds[cloud];
}
