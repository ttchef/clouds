
#include "light.h"
#include "texture.h"
#include "vk/command.h"
#include "vk/descriptor.h"
#include "vk/sampler.h"
#include "vk/swapchain.h"
#include <log.h>
#include <renderer.h>
#include <vulkan/vulkan_core.h>

static bool create_pipelines(struct renderer *r) { return true; }

static void destroy_pipelines(struct renderer *r) {}

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

    LOGM(INFO, "created vulkan descriptor sets");

    if (!texture_manager_create(r, &r->texture_manager)) {
        return false;
    }

    LOGM(INFO, "created texture manager");

    if (!light_manager_create(r, &r->light_manager)) {
        return false;
    }

    LOGM(INFO, "created light manager");

    if (!create_pipelines(r)) {
        return false;
    }

    LOGM(INFO, "created vulkan pipelines");

    if (!vk_command_create(&r->init, &r->cmd)) {
        return false;
    }

    LOGM(INFO, "created vulkan command resources");

    return true;
}

void renderer_deint(struct renderer *r) {
    destroy_pipelines(r);

    light_manager_destroy(r, &r->light_manager);
    texture_manager_destroy(r, &r->texture_manager);

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
