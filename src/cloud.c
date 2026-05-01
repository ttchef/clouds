
#include <cloud.h>
#include <log.h>
#include <renderer.h>

#include <string.h>

void cloud_manager_init(struct cloud_manager *manager) {
    memset(manager, 0, sizeof(*manager));
    manager->selected_cloud = NO_CLOUD;
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

        id = i;
        break;
    }

    return id;
}

struct cloud *cloud_get(struct renderer *r, cloud_id cloud) {
    if (cloud < 0 || cloud >= MAX_CLOUDS) {
        LOGM(WARN, "cloud id is invalid: %d", cloud);
        return NULL;
    }

    struct cloud_manager *m = &r->cloud_manager;

    return &m->clouds[cloud];
}
