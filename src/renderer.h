
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

struct image {
    VkImage handle;
    VkImageView view;
    VmaAllocation alloc;
};

struct swapchain {
    VkSwapchainKHR handle;
    VkFormat fmt;
    VkExtent2D extent;

    // not of type struct image because they dont
    // need allocated memory
    VkImageView *imgs_views;
    VkImage *imgs;

    struct image *depth_images;
    u32 n_imgs;
};

struct pipeline {
    VkPipeline handle;
    VkPipelineLayout layout;

    VkDescriptorPool desc_pool;
    VkDescriptorSetLayout desc_layout;
    VkDescriptorSet desc_sets[FRAMES_IN_FLIGHT];
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

typedef u32 model_id;

struct model {
    struct buffer vertex_buffer;
    struct buffer index_buffer;
    u32 n_index;

    struct image image;
    bool has_image;
};

enum {
    DRAW_CMD_TYPE_BOX,
    DRAW_CMD_TYPE_MODEL_COLOR,
    DRAW_CMD_TYPE_MODEL_TEXTURE,
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
            model_id id;
            vec4 color;
        } model_color;

        struct {
            model_id id;
        } model_texture;
    };
};

struct render_queue {
    u32 count;
    u32 capacity;
    struct draw_cmd *cmds;
};

struct camera {
    f32 speed;
    f32 sensitivity;

    f32 yaw;
    f32 pitch;

    vec2 last_mouse;
    vec3 pos;
    vec3 direction;

    bool invis_cursor;
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

    struct pipeline model_color_pip;
    struct pipeline model_texture_pip;

    VkCommandPool cmd_pool;
    struct frame_data frame_data[FRAMES_IN_FLIGHT];
    VkSemaphore finished[FRAMES_IN_FLIGHT];
    u32 frame_idx;
    u32 img_idx;

    // dynamic array (darray.h)
    struct model *models;
    model_id box_id;

    struct render_queue render_queue;
    struct camera cam;
    VkSampler sampler;
};

bool renderer_init(struct rcontext *rctx, GLFWwindow *window, i32 n_exts,
                   const char **exts, i32 n_layers, const char **layers);

bool renderer_resize(struct rcontext *rctx, u32 w, u32 h);

void renderer_push_box(struct rcontext *rctx, vec3 pos, vec3 scale, vec4 color);

// renders the model in the color specified
void renderer_push_model_color(struct rcontext *rctx, vec3 pos, vec3 scale,
                               vec4 color, model_id model);

void renderer_push_model_texture(struct rcontext *rctx, vec3 pos, vec3 scale,
                                 model_id model);

bool renderer_draw(struct rcontext *rctx, GLFWwindow *window);

void renderer_update_cam(struct rcontext *rctx, GLFWwindow *window, f32 dt);

model_id renderer_create_model(struct rcontext *rctx, const char *filepath);

void renderer_destroy_model(struct rcontext *rctx, model_id id);

void renderer_deint(struct rcontext *rctx);

#endif // RENDERER_H
