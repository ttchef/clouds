
#include "types.h"
#include "vk/image.h"
#include <vk/init.h>
#include <vk/swapchain.h>

#include <cmath.h>
#include <log.h>
#include <vulkan/vulkan_core.h>

static bool build_swapchain(struct vk_init *init,
                            struct vk_swapchain *swapchain, u32 w, u32 h,
                            VkSwapchainKHR old_handle) {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(init->phy_dev, init->surface,
                                              &caps);

    u32 n_fmts;
    vkGetPhysicalDeviceSurfaceFormatsKHR(init->phy_dev, init->surface, &n_fmts,
                                         NULL);
    VkSurfaceFormatKHR fmts[n_fmts];
    vkGetPhysicalDeviceSurfaceFormatsKHR(init->phy_dev, init->surface, &n_fmts,
                                         fmts);

    VkSurfaceFormatKHR fmt = fmts[0];
    for (u32 i = 0; i < n_fmts; i++) {
        if (fmts[i].format == VK_FORMAT_B8G8R8_SRGB &&
            fmts[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            fmt = fmts[i];
            break;
        }
    }

    u32 n_present_modes;
    vkGetPhysicalDeviceSurfacePresentModesKHR(init->phy_dev, init->surface,
                                              &n_present_modes, NULL);
    VkPresentModeKHR present_modes[n_present_modes];
    vkGetPhysicalDeviceSurfacePresentModesKHR(init->phy_dev, init->surface,
                                              &n_present_modes, present_modes);

    VkPresentModeKHR present_mode = present_modes[0];
    for (u32 i = 0; i < n_present_modes; i++) {
        if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            present_mode = present_modes[i];
            break;
        }
    }

    VkExtent2D extent = (VkExtent2D){
        .width = w,
        .height = h,
    };

    extent.width = MIN(caps.maxImageExtent.width, extent.width);
    extent.height = MIN(caps.maxImageExtent.height, extent.height);
    extent.width = MAX(caps.minImageExtent.width, extent.width);
    extent.height = MAX(caps.minImageExtent.height, extent.height);

    u32 n_imgs = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && n_imgs > caps.maxImageCount) {
        n_imgs = caps.maxImageCount;
    }
    LOGM(API_DUMP, "swapchain image count: %d", n_imgs);

    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .oldSwapchain = old_handle,
        .surface = init->surface,
        .clipped = VK_TRUE,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageFormat = fmt.format,
        .imageColorSpace = fmt.colorSpace,
        .presentMode = present_mode,
        .imageExtent = extent,
        .minImageCount = n_imgs,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
    };

    if (vkCreateSwapchainKHR(init->dev, &create_info, NULL,
                             &swapchain->handle) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create swapchain");
        return false;
    }

    LOGM(API_DUMP, "swapchain size: %dx%d", extent.width, extent.height);

    swapchain->fmt = fmt.format;
    swapchain->extent = extent;

    vkGetSwapchainImagesKHR(init->dev, swapchain->handle, &swapchain->n_imgs,
                            NULL);
    swapchain->imgs = calloc(swapchain->n_imgs, sizeof(VkImage));
    vkGetSwapchainImagesKHR(init->dev, swapchain->handle, &swapchain->n_imgs,
                            swapchain->imgs);

    swapchain->imgs_views = calloc(swapchain->n_imgs, sizeof(VkImageView));
    swapchain->depth_images =
        calloc(swapchain->n_imgs, sizeof(struct vk_image));

    for (u32 i = 0; i < swapchain->n_imgs; i++) {
        VkImageViewCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchain->imgs[i],
            .format = swapchain->fmt,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .layerCount = 1,
                    .levelCount = 1,
                },
        };

        if (vkCreateImageView(init->dev, &create_info, NULL,
                              &swapchain->imgs_views[i]) != VK_SUCCESS) {
            LOGM(ERROR, "Failed to create image view: %d", i);

            free(swapchain->imgs_views);
            swapchain->imgs_views = NULL;

            free(swapchain->imgs);
            swapchain->imgs = NULL;

            return false;
        }

        vk_image_create(
            init, &swapchain->depth_images[i], w, h, 1, VK_FORMAT_D32_SFLOAT,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, IMAGE_TYPE_2D);
    }

    VkSemaphoreCreateInfo sem_create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    swapchain->finished = malloc(sizeof(VkSemaphore) * swapchain->n_imgs);
    for (u32 i = 0; i < swapchain->n_imgs; i++) {
        if (vkCreateSemaphore(init->dev, &sem_create_info, NULL,
                              &swapchain->finished[i]) != VK_SUCCESS) {
            LOGM(ERROR, "failed to create image availabe semaphore: %d", i);
            return false;
        }
    }

    swapchain->img_idx = 0;

    return true;
}

bool vk_swapchain_create(struct vk_init *init, struct vk_swapchain *swapchain,
                         u32 w, u32 h) {
    swapchain->graveyard_count = 0;
    return build_swapchain(init, swapchain, w, h, VK_NULL_HANDLE);
}

bool vk_swapchain_recreate(struct vk_init *init, struct vk_swapchain *swapchain,
                           u32 width, u32 height, u64 frame_idx) {
    if (swapchain->graveyard_count >= MAX_SWAPCHAIN_GRAVEYARD_SIZE) {
        LOGM(WARN, "swapchain graveyard is full");
        return false;
    }

    struct vk_swapchain_zombie *z =
        &swapchain->graveyard[swapchain->graveyard_count++];
    z->handle = swapchain->handle;
    z->imgs_views = swapchain->imgs_views;
    z->imgs = swapchain->imgs;
    z->depth_images = swapchain->depth_images;
    z->finished = swapchain->finished;
    z->n_imgs = swapchain->n_imgs;
    z->frame_retired = frame_idx;

    // null out old swapchain
    swapchain->handle = VK_NULL_HANDLE;
    swapchain->imgs_views = NULL;
    swapchain->imgs = NULL;
    swapchain->depth_images = NULL;
    swapchain->finished = NULL;

    return build_swapchain(init, swapchain, width, height, z->handle);
}

void vk_swapchain_drain(struct vk_init *init, struct vk_swapchain *swapchain,
                        u64 frame_idx) {
    for (u32 i = 0; i < swapchain->graveyard_count;) {
        struct vk_swapchain_zombie *z = &swapchain->graveyard[i];

        if (frame_idx - z->frame_retired < (u64)FRAMES_IN_FLIGHT) {
            i++;
            continue;
        }

        for (u32 j = 0; j < z->n_imgs; j++) {
            vkDestroyImageView(init->dev, z->imgs_views[j], NULL);
            vk_image_destroy(init, &z->depth_images[j]);
            vkDestroySemaphore(init->dev, z->finished[j], NULL);
        }

        free(z->imgs_views);
        free(z->imgs);
        free(z->depth_images);
        free(z->finished);
        vkDestroySwapchainKHR(init->dev, z->handle, NULL);

        // swap and remove
        swapchain->graveyard[i] =
            swapchain->graveyard[--swapchain->graveyard_count];
    }
}

void vk_swapchain_destroy(struct vk_init *init,
                          struct vk_swapchain *swapchain) {
    if (swapchain->imgs) {
        free(swapchain->imgs);
    }

    if (swapchain->imgs_views) {
        for (u32 i = 0; i < swapchain->n_imgs; i++) {
            vkDestroyImageView(init->dev, swapchain->imgs_views[i], NULL);
        }
        free(swapchain->imgs_views);
    }

    if (swapchain->depth_images) {
        for (u32 i = 0; i < swapchain->n_imgs; i++) {
            vk_image_destroy(init, &swapchain->depth_images[i]);
        }
        free(swapchain->depth_images);
    }

    if (swapchain->finished) {
        for (u32 i = 0; i < swapchain->n_imgs; i++) {
            vkDestroySemaphore(init->dev, swapchain->finished[i], NULL);
        }

        free(swapchain->finished);
    }

    vkDestroySwapchainKHR(init->dev, swapchain->handle, NULL);
}
