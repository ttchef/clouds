
#ifndef RENDERER_H
#define RENDERER_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "types.h"

struct api_version {
    u32 major;
    u32 minor;
    u32 patch;
};

struct queue {
    VkQueue handle;
    i32 index;
};

struct swapchain {
    VkSwapchainKHR handle;
    VkFormat fmt;
    VkExtent2D extent;

    VkImageView *imgs_views;
    VkImage *imgs;
    u32 n_imgs;
};

struct rcontext {
    VkInstance instance;
    VkDebugUtilsMessengerEXT db_messenger;
    VkSurfaceKHR surface;
    VkPhysicalDevice phy_dev;
    VkDevice dev;
    struct queue graphics_queue;
    struct queue present_queue;
    struct swapchain swapchain;
};

bool renderer_init(struct rcontext *rctx, GLFWwindow *window, i32 n_exts,
                   const char **exts, i32 n_layers, const char **layers);
void renderer_deint(struct rcontext *rctx);

#endif // RENDERER_H
