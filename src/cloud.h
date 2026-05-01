
#ifndef CLOUD_H
#define CLOUD_H

#include <cmath.h>
#include <types.h>

struct cloud {
    vec3 pos;
    vec3 scale;

    bool render_bounding_box;
};

struct cloud cloud_create(vec3 pos, vec3 scale);

void cloud_render_bounding_box(struct cloud *cloud, bool state);

#endif // CLOUD_H
