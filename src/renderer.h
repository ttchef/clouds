
#ifndef RENDERER_H
#define RENDERER_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vma/vma.h>

#include "cmath.h"
#include "types.h"

#define FRAMES_IN_FLIGHT 3

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

struct pipeline {
    VkPipeline handle;
    VkPipelineLayout layout;
};

struct buffer {
    VkBuffer handle;
    VmaAllocation alloc;
    VkDeviceSize size;
};

struct frame_data {
    VkSemaphore image_available;
    VkFence in_flight_fence;
    VkCommandBuffer cmd_buffer;
};

typedef u32 texture_id;

enum {
    DRAW_CMD_TYPE_BOX,
    DRAW_CMD_TYPE_TEXTURE_BOX,
};

struct draw_cmd {
    i32 type;
    vec3 pos;
    vec3 scale;

    union {
        struct {
            vec4 color;
        } box;

        struct {
            texture_id id;
        } texture_box;
    };
};

struct box_push_constant {
    matrix m;
    vec4 color;
};

struct render_queue {
    u32 count;
    u32 capacity;
    struct draw_cmd *cmds;
};

struct rcontext {
    VmaAllocator allocator;
    VkInstance instance;
    VkDebugUtilsMessengerEXT db_messenger;
    VkSurfaceKHR surface;
    VkPhysicalDevice phy_dev;
    VkDevice dev;

    struct queue graphics_queue;
    struct queue present_queue;
    struct swapchain swapchain;
    struct pipeline pipeline;

    VkCommandPool cmd_pool;
    struct frame_data frame_data[FRAMES_IN_FLIGHT];
    VkSemaphore finished[FRAMES_IN_FLIGHT];
    u32 frame_idx;
    u32 img_idx;

    struct buffer vertex_buffer;
    struct render_queue render_queue;
};

bool renderer_init(struct rcontext *rctx, GLFWwindow *window, i32 n_exts,
                   const char **exts, i32 n_layers, const char **layers);
bool renderer_resize(struct rcontext *rctx, u32 w, u32 h);
void renderer_push_box(struct rcontext *rctx, vec3 pos, vec3 scale, vec4 color);
bool renderer_draw(struct rcontext *rctx, GLFWwindow *window);
void renderer_deint(struct rcontext *rctx);

#endif // RENDERER_H
