
#ifndef VULKAN_SWAPCHAIN_H
#define VULKAN_SWAPCHAIN_H

#include <types.h>
#include <vk/image.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

struct vk_init;

#define MAX_SWAPCHAIN_GRAVEYARD_SIZE 4

struct vk_swapchain_zombie {
    VkSwapchainKHR handle;
    VkImageView *imgs_views;
    VkImage *imgs;

    struct vk_image *depth_images;
    VkSemaphore *finished;

    u32 n_imgs;
    u64 frame_retired;
};

struct vk_swapchain {
    VkSwapchainKHR handle;

    VkFormat fmt;
    VkExtent2D extent;

    // not of type struct image because they dont
    // need allocated memory
    VkImageView *imgs_views;
    VkImage *imgs;

    struct vk_image *depth_images;
    u32 n_imgs;

    VkSemaphore *finished;
    u32 img_idx;

    // old swapchain "graveyard"
    struct vk_swapchain_zombie graveyard[MAX_SWAPCHAIN_GRAVEYARD_SIZE];
    u32 graveyard_count;
};

bool vk_swapchain_create(struct vk_init *init, struct vk_swapchain *swapchain,
                         u32 w, u32 h);

void vk_swapchain_drain(struct vk_init *init, struct vk_swapchain *swapchain,
                        u64 frame_idx_not_cleared);

bool vk_swapchain_recreate(struct vk_init *init, struct vk_swapchain *swapchain,
                           u32 width, u32 height, u64 frame_idx_not_cleared);

void vk_swapchain_destroy(struct vk_init *init, struct vk_swapchain *swapchain);

#endif // VULKAN_SWAPCHAIN_H
