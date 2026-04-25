
#ifndef VULKAN_SAMPLER_H
#define VULKAN_SAMPLER_H

#include <types.h>

#include <vulkan/vulkan.h>

struct vk_init;

struct vk_sampler {
    VkSampler handle;
};

struct vk_samplers {
    // will add more in the future probaly
    struct vk_sampler texture_sampler;
};

bool vk_samplers_create(struct vk_init *init, struct vk_samplers *samplers);
void vk_samplers_destroy(struct vk_init *init, struct vk_samplers *samplers);

#endif // VULKAN_SAMPLER_H
