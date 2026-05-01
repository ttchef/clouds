
#ifndef CLOUD_H
#define CLOUD_H

#include <cmath.h>
#include <types.h>

#define MAX_CLOUDS 10
#define NO_CLOUD -1

typedef i32 cloud_id;

struct renderer;

struct cloud {
    vec3 pos;
    vec3 scale;

    bool valid;
};

struct cloud_manager {
    struct cloud clouds[MAX_CLOUDS];
    cloud_id selected_cloud;
};

void cloud_manager_init(struct cloud_manager *manager);

cloud_id cloud_create(struct renderer *r, vec3 pos, vec3 scale);

struct cloud *cloud_get(struct renderer *r, cloud_id cloud);

#endif // CLOUD_H
