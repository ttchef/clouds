
#ifndef VULKAN_SWAPCHAIN_H
#define VULKAN_SWAPCHAIN_H

#include <types.h>
#include <vk/image.h>

#include <vulkan/vulkan.h>

struct vk_init;

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
};

bool vk_swapchain_create(struct vk_init *init, struct vk_swapchain *swapchain,
                         u32 w, u32 h);
void vk_swapchain_destroy(struct vk_swapchain *swapchain);

#endif // VULKAN_SWAPCHAIN_H
