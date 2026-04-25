
#ifndef VULKAN_INIT_H
#define VULKAN_INIT_H

#include <types.h>

#include <vma/vma.h>
#include <vulkan/vulkan.h>

struct window;

struct vk_api_version {
    u32 major;
    u32 minor;
    u32 patch;
};

struct vk_queue {
    VkQueue handle;
    i32 index;
};

struct vk_init {
    VmaAllocator allocator;
    VkInstance instance;
    VkDebugUtilsMessengerEXT db_messenger;
    VkSurfaceKHR surface;
    VkPhysicalDevice phy_dev;
    VkDevice dev;

    struct vk_queue present_queue;
    struct vk_queue graphics_queue;
};

struct vk_api_version init_get_api_version();
bool init_vk(struct vk_init *init, struct window *window);
void deinit_vk(struct vk_init *init);

#endif // VULKAN_INIT_H
