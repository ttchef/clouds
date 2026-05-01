
#include <cloud.h>

struct cloud cloud_create(vec3 pos, vec3 scale) {
    struct cloud res = {
        .pos = pos,
        .scale = scale,
    };

    return res;
}

void cloud_render_bounding_box(struct cloud *cloud, bool state) {
    cloud->render_bounding_box = state;
}
