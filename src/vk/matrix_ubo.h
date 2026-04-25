
#ifndef VULKAN_MATRIX_UBO
#define VULKAN_MATRIX_UBO

#include <cmath.h>

#include <vk/buffer.h>

struct renderer;

struct vk_matrix_ubo_data {
    matrix proj;
    matrix view;
    matrix proj_view;
};

struct vk_matrix_ubo {
    struct vk_buffer buffers[FRAMES_IN_FLIGHT];
    struct vk_matrix_ubo_data data;
};

bool vk_matrix_ubo_create(struct renderer *r, struct vk_matrix_ubo *m);

void vk_matrix_ubo_destroy(struct renderer *r, struct vk_matrix_ubo *m);

// send data to gpu
void vk_matrix_ubo_sync_data(struct renderer *r, struct vk_matrix_ubo *m);

#endif // VULKAN_MATRIX_UBO
