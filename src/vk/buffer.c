
#include <vk/buffer.h>
#include <vk/init.h>

#include <log.h>

#include <string.h>

struct vk_buffer vk_buffer_create_device_local(struct vk_init *init,
                                               VkDeviceSize size,
                                               VkBufferUsageFlags flags) {
    struct vk_buffer res = {
        .type = BUFFER_TYPE_DEVICE_LOCAL,
        .size = size,
    };

    VkBufferCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .usage = flags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .size = size,
    };

    VmaAllocationCreateInfo alloc_info = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };

    if (vmaCreateBuffer(init->allocator, &create_info, &alloc_info, &res.handle,
                        &res.alloc, NULL) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create device local buffer");
        return (struct vk_buffer){0};
    }

    return res;
}

struct vk_buffer vk_buffer_create_host_visible(struct vk_init *init,
                                               VkDeviceSize size,
                                               VkBufferUsageFlags flags) {
    struct vk_buffer res = {
        .type = BUFFER_TYPE_HOST_VISIBLE,
        .size = size,
    };

    VkBufferCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .usage = flags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .size = size,
    };

    VmaAllocationCreateInfo alloc_info = {
        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
    };

    VmaAllocationInfo alloc_out;

    if (vmaCreateBuffer(init->allocator, &create_info, &alloc_info, &res.handle,
                        &res.alloc, &alloc_out) != VK_SUCCESS) {
        LOGM(ERROR, "failed to host visible buffer");
        return (struct vk_buffer){0};
    }

    res.host_visible.mapped = alloc_out.pMappedData;

    return res;
}

struct vk_buffer vk_buffer_create_staging(struct vk_init *init,
                                          VkDeviceSize size, const void *data) {
    struct vk_buffer res = {
        .type = BUFFER_TYPE_STAGING,
        .size = size,
    };

    VkBufferCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .usage =
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .size = size,
    };

    VmaAllocationCreateInfo alloc_info = {
        .usage = VMA_MEMORY_USAGE_CPU_ONLY,
    };

    if (vmaCreateBuffer(init->allocator, &create_info, &alloc_info, &res.handle,
                        &res.alloc, NULL) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create device local buffer");
        return (struct vk_buffer){0};
    }

    void *mapped;
    vmaMapMemory(init->allocator, res.alloc, &mapped);
    memcpy(mapped, data, size);
    vmaUnmapMemory(init->allocator, res.alloc);

    return res;
}

bool vk_buffer_copy(struct vk_init *init, struct vk_buffer *staging,
                    struct vk_buffer *buffer) {
    // dont handle at the moment
    if (buffer->size != staging->size) {
        LOGM(ERROR, "buffer size and staing buffer size are not equal");
        return false;
    }

    VkCommandPool cmd_pool;
    VkCommandBuffer cmd_buffer;

    VkCommandPoolCreateInfo pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = init->graphics_queue.index,
    };

    if (vkCreateCommandPool(init->dev, &pool_create_info, NULL, &cmd_pool) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to create one time use command pool for staging "
                    "buffer copy");
        return false;
    }

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = cmd_pool,
        .commandBufferCount = 1,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    };

    if (vkAllocateCommandBuffers(init->dev, &alloc_info, &cmd_buffer) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to create commnad buffer for staging buffer copy");
        return false;
    }

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    if (vkBeginCommandBuffer(cmd_buffer, &begin_info) != VK_SUCCESS) {
        LOGM(
            ERROR,
            "failed to begin recording command buffer for staging buffer copy");
        vkDestroyCommandPool(init->dev, cmd_pool, NULL);
        return false;
    }

    VkBufferCopy region = {
        .size = buffer->size,
    };

    vkCmdCopyBuffer(cmd_buffer, staging->handle, buffer->handle, 1, &region);

    if (vkEndCommandBuffer(cmd_buffer) != VK_SUCCESS) {
        LOGM(ERROR,
             "failed to end recording command buffer for staging buffer copy");
        vkDestroyCommandPool(init->dev, cmd_pool, NULL);
        return false;
    }

    VkSubmitInfo sub_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_buffer,
    };

    if (vkQueueSubmit(init->graphics_queue.handle, 1, &sub_info,
                      VK_NULL_HANDLE) != VK_SUCCESS) {
        LOGM(ERROR, "failed to submit command buffer for staging buffer copy");
        vkDestroyCommandPool(init->dev, cmd_pool, NULL);
        return false;
    }

    if (vkQueueWaitIdle(init->graphics_queue.handle) != VK_SUCCESS) {
        LOGM(ERROR, "failed to wait for queues");
        vkDestroyCommandPool(init->dev, cmd_pool, NULL);
        return false;
    }

    vkDestroyCommandPool(init->dev, cmd_pool, NULL);

    return true;
}

bool vk_buffer_upload_data(struct vk_init *init, struct vk_buffer *buffer,
                           u32 data_size, void *data) {
    if (data_size != buffer->size) {
        LOGM(WARN, "buffer->size != data_size");
        return false;
    }

    struct vk_buffer staging = vk_buffer_create_staging(init, data_size, data);
    vk_buffer_copy(init, &staging, buffer);
    vmaDestroyBuffer(init->allocator, staging.handle, staging.alloc);

    return true;
}
