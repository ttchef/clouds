
#include "vk/init.h"
#include <log.h>
#include <renderer.h>

bool renderer_init(struct renderer *r, struct window *window) {
    if (!init_vk(&r->init, window)) {
        return false;
    }

    LOGM(INFO, "initialized basic vulkan resources");

    return true;
}

void renderer_deint(struct renderer *r) { deinit_vk(&r->init); }
