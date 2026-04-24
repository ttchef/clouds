
#ifndef VULKAN_BUFFER_H
#define VULKAN_BUFFER_H

#include <types.h>

#include <vma/vma.h>
#include <vulkan/vulkan.h>

enum {
    BUFFER_TYPE_DEVICE_LOCAL,
    BUFFER_TYPE_HOST_VISIBLE,
    BUFFER_TYPE_STAGING,
};

struct vk_init;

struct vk_buffer {
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

struct vk_buffer vk_buffer_create_device_local(struct vk_init *init,
                                               VkDeviceSize size,
                                               VkBufferUsageFlags flags);

struct vk_buffer vk_buffer_create_host_visible(struct vk_init *init,
                                               VkDeviceSize size,
                                               VkBufferUsageFlags flags);

struct vk_buffer vk_buffer_create_staging(struct vk_init *init,
                                          VkDeviceSize size, const void *data);

bool vk_buffer_copy(struct vk_init *init, struct vk_buffer *staging,
                    struct vk_buffer *buffer);

bool vk_buffer_upload_data(struct vk_init *init, struct vk_buffer *buffer,
                           u32 data_size, void *data);

#endif // VULKAN_BUFFER_H
