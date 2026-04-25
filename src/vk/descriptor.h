
#ifndef VULKAN_DESCRIPTOR_H
#define VULKAN_DESCRIPTOR_H

#include <types.h>

#include <vulkan/vulkan.h>

struct vk_init;

struct vk_descriptor {
    VkDescriptorPool pool;
    VkDescriptorSetLayout layout;
    VkDescriptorSet sets[FRAMES_IN_FLIGHT];
};

bool vk_descriptor_create(struct vk_init *init, struct vk_descriptor *desc);

void vk_descriptor_destroy(struct vk_init *init, struct vk_descriptor *desc);

#endif // VULKAN_DESCRIPTOR_H
