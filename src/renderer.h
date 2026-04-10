
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
#define NO_MODEL -1
#define NO_TEXTURE -1
#define NO_LIGHT -1

// total max of different textures to exist
#define MAX_TEXTURES 1024
#define MAX_LIGHTS 256

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
};

enum {
    BUFFER_TYPE_DEVICE_LOCAL,
    BUFFER_TYPE_HOST_VISIBLE,
    BUFFER_TYPE_STAGING,
};

struct buffer {
    i32 type;

    VkBuffer handle;
    VmaAllocation alloc;
    VkDeviceSize size;

    union {
        struct host_visible {
            void *mapped;
        } host_visible;
    };
};

struct frame_data {
    VkSemaphore image_available;
    VkFence in_flight_fence;
    VkCommandBuffer cmd_buffer;
};

// descriptor handler
struct global_desc {
    VkDescriptorPool pool;
    VkDescriptorSetLayout layout;
    VkDescriptorSet sets[FRAMES_IN_FLIGHT];
};

typedef i32 texture_id;
typedef i32 model_id;
typedef i32 light_id;

struct texture {
    struct image image;
    bool valid;
};

struct texture_manager {
    struct texture textures[MAX_TEXTURES];
};

// needs to be with valid alignement
// for gpu uniform buffers
struct light {
    vec4 pos;
    vec4 direction;
    vec4 color;
};

// watch out for alignement
struct light_buffer {
    struct light lights[MAX_LIGHTS];
    u32 count;
    u32 padding[3];
};

// works the same as the texture manager
struct light_manager {
    struct buffer buffers[FRAMES_IN_FLIGHT];

    // + 1 for the count at the end
    struct light lights[MAX_LIGHTS + 1];
    bool valid[MAX_LIGHTS];
    u32 count;

    struct light_buffer light_buffer;
};

struct matrix_ubo {
    struct buffer buffers[FRAMES_IN_FLIGHT];
};

struct model {
    struct buffer vertex_buffer;
    struct buffer index_buffer;
    u32 n_index;

    texture_id texture;
    bool valid;
};

enum {
    DRAW_CMD_TYPE_MODEL_COLOR,
    DRAW_CMD_TYPE_MODEL_TEXTURE,
};

struct draw_cmd {
    i32 type;
    vec3 pos;
    vec3 scale;

    union {
        struct {
            model_id id;
            vec4 color;
        } model_color;

        struct {
            model_id id;

            // only for special purposes
            // like the box where the model
            // doesnt have the texture itself
            texture_id texture;
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

    // heap allocated array (swapchain_images count)
    VkSemaphore *finished;
    u32 frame_idx;
    u32 img_idx;

    // dynamic array (darray.h)
    struct model *models;
    model_id box_id;

    struct global_desc descriptors;
    struct texture_manager texture_manager;
    struct light_manager light_manager;

    struct matrix_ubo matrix_ubo;

    struct render_queue render_queue;
    struct camera cam;
    VkSampler sampler;
};

bool renderer_init(struct rcontext *rctx, GLFWwindow *window, i32 n_exts,
                   const char **exts, i32 n_layers, const char **layers);

bool renderer_resize(struct rcontext *rctx, u32 w, u32 h);

void renderer_push_box(struct rcontext *rctx, vec3 pos, vec3 scale, vec4 color,
                       texture_id texture);

// renders the model in the color specified
void renderer_push_model_color(struct rcontext *rctx, vec3 pos, vec3 scale,
                               vec4 color, model_id model);

void renderer_push_model_texture(struct rcontext *rctx, vec3 pos, vec3 scale,
                                 model_id model);

bool renderer_draw(struct rcontext *rctx, GLFWwindow *window);

void renderer_update_cam(struct rcontext *rctx, GLFWwindow *window, f32 dt);

texture_id renderer_create_texture(struct rcontext *rctx, const char *filepath);

void renderer_destroy_texture(struct rcontext *c, texture_id id);

model_id renderer_create_model(struct rcontext *rctx, const char *filepath);

void renderer_destroy_model(struct rcontext *rctx, model_id id);

bool renderer_set_model_texture(struct rcontext *rctx, model_id model,
                                texture_id texture);

light_id renderer_create_light(struct rcontext *rctx, vec3 pos, vec3 direction,
                               vec3 color);

void renderer_destroy_light(struct rcontext *rctx, light_id id);

void renderer_set_light_state(struct rcontext *rctx, light_id id, bool on);

void renderer_update_light(struct rcontext *rctx, light_id id, vec3 pos,
                           vec3 direction, vec3 color);

bool renderer_update(struct rcontext *rctx, f32 dt);

void renderer_deint(struct rcontext *rctx);

#endif // RENDERER_H
