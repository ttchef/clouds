
#ifndef VULKAN_COMMAND_H
#define VULKAN_COMMAND_H

// this file handles command pools and buffers
// as well as the frame data

#include <types.h>

#include <vulkan/vulkan.h>

struct vk_init;
struct renderer;

struct vk_frame_data {
    VkSemaphore image_available;
    VkFence in_flight_fence;
    VkCommandBuffer cmd_buffer;
};

struct vk_command {
    VkCommandPool cmd_pool;
    struct vk_frame_data frame_data[FRAMES_IN_FLIGHT];

    u32 frame_idx;
    u64 frame_idx_not_cleared;
};

bool vk_command_create(struct vk_init *init, struct vk_command *cmd);

bool vk_command_record(struct renderer *r);

void vk_command_destroy(struct vk_init *init, struct vk_command *cmd);

#endif // VULKAN_COMMAND_H
