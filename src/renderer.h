
#ifndef RENDERER_H
#define RENDERER_H

#include <vk/descriptor.h>
#include <vk/init.h>
#include <vk/sampler.h>
#include <vk/swapchain.h>
#include <window.h>

struct renderer {
    struct vk_init init;
    struct vk_swapchain swapchain;
    struct vk_samplers samplers;
    struct vk_descriptor descriptors;
};

bool renderer_init(struct renderer *r, struct window *window);
void renderer_deint(struct renderer *r);

bool renderer_resize(struct renderer *r, struct window *window);

#endif // RENDERER_H
