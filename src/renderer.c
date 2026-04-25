
#include "vk/descriptor.h"
#include "vk/sampler.h"
#include "vk/swapchain.h"
#include <log.h>
#include <renderer.h>
#include <vulkan/vulkan_core.h>

bool renderer_init(struct renderer *r, struct window *window) {
    if (!init_vk(&r->init, window)) {
        return false;
    }

    LOGM(INFO, "initialized basic vulkan resources");

    if (!vk_swapchain_create(&r->init, &r->swapchain, window->width,
                             window->height)) {
        return false;
    }

    LOGM(INFO, "created vulkan swapchain");

    if (!vk_samplers_create(&r->init, &r->samplers)) {
        return false;
    }

    LOGM(INFO, "created vulkan samplers");

    if (!vk_descriptor_create(&r->init, &r->descriptors)) {
        return false;
    }

    LOGM(INFO, "created descriptor sets");

    return true;
}

void renderer_deint(struct renderer *r) {
    vk_descriptor_destroy(&r->init, &r->descriptors);
    vk_samplers_destroy(&r->init, &r->samplers);
    vk_swapchain_destroy(&r->init, &r->swapchain);
    deinit_vk(&r->init);
}

bool renderer_resize(struct renderer *r, struct window *window) {
    vkDeviceWaitIdle(r->init.dev);

    vk_swapchain_destroy(&r->init, &r->swapchain);
    if (!vk_swapchain_create(&r->init, &r->swapchain, window->width,
                             window->height)) {
        return false;
    }

    return true;
}
