
#ifndef VULKAN_IMAGE_H
#define VULKAN_IMAGE_H

#include <types.h>

#include <vma/vma.h>
#include <vulkan/vulkan.h>

enum {
    IMAGE_TYPE_2D,
    IMAGE_TYPE_3D,
    IMAGE_TYPE_CUBE_MAP,
};

struct vk_init;

struct vk_image {
    VkImage handle;
    VkImageView view;
    VmaAllocation alloc;
    i32 type;
};

bool vk_image_create(struct vk_init *init, struct vk_image *image, u32 width,
                     u32 height, u32 depth, VkFormat fmt,
                     VkImageUsageFlags usage, i32 type);

bool vk_image_create_cube_map(struct vk_init *init, struct vk_image *image,
                              const char *path);

bool vk_image_create_noise(struct vk_init *init, struct vk_image *image);

bool vk_image_transition(struct vk_init *init, struct vk_image *image,
                         VkImageLayout old_layout, VkImageLayout new_layout,
                         VkAccessFlags src_access, VkAccessFlags dst_access,
                         VkPipelineStageFlags src_stage,
                         VkPipelineStageFlags dst_stage,
                         VkImageAspectFlags aspect_mask);

bool vk_image_upload_data(struct vk_init *init, struct vk_image *image,
                          u32 data_size, void *data, u32 width, u32 height,
                          u32 depth, VkImageLayout final_layout,
                          VkAccessFlags dst_access_mask, i32 *cube_face);

void vk_image_destroy(struct vk_init *init, struct vk_image *image);

#endif // VULKAN_IMAGE_H
