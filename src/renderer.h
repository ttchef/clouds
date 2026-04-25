
#ifndef RENDERER_H
#define RENDERER_H

#include <vk/command.h>
#include <vk/descriptor.h>
#include <vk/init.h>
#include <vk/pipeline.h>
#include <vk/sampler.h>
#include <vk/swapchain.h>

#include <light.h>
#include <texture.h>
#include <window.h>

struct renderer {
    struct vk_init init;
    struct vk_swapchain swapchain;
    struct vk_samplers samplers;
    struct vk_descriptor descriptors;

    struct vk_pipeline model_color_pip;
    struct vk_pipeline model_texture_pip;
    struct vk_pipeline skybox_pip;
    struct vk_pipeline cloud_pip;

    struct vk_command cmd;

    struct texture_manager texture_manager;
    struct light_manager light_manager;
};

bool renderer_init(struct renderer *r, struct window *window);
void renderer_deint(struct renderer *r);

bool renderer_resize(struct renderer *r, struct window *window);

#endif // RENDERER_H
