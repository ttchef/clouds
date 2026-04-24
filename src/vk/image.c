
#include <vk/buffer.h>
#include <vk/image.h>
#include <vk/init.h>

#include <cmath.h>
#include <log.h>

#include <FastNoiseLite/FastNoiseLite.h>
#include <stbi/stb_image.h>

enum {
    CUBE_MAP_X_POS,
    CUBE_MAP_X_NEG,
    CUBE_MAP_Y_POS,
    CUBE_MAP_Y_NEG,
    CUBE_MAP_Z_POS,
    CUBE_MAP_Z_NEG,
};

bool vk_image_create(struct vk_init *init, struct vk_image *image, u32 width,
                     u32 height, u32 depth, VkFormat fmt,
                     VkImageUsageFlags usage, i32 type) {
    VkImageType image_type;
    VkImageViewType view_type;

    switch (type) {
    case IMAGE_TYPE_2D:
        image_type = VK_IMAGE_TYPE_2D;
        view_type = VK_IMAGE_VIEW_TYPE_2D;
        break;
    case IMAGE_TYPE_3D:
        image_type = VK_IMAGE_TYPE_3D;
        view_type = VK_IMAGE_VIEW_TYPE_3D;
        break;
    case IMAGE_TYPE_CUBE_MAP:
        image_type = VK_IMAGE_TYPE_2D;
        view_type = VK_IMAGE_VIEW_TYPE_CUBE;
        break;
    default:
        LOGM(ERROR, "invalid image type");
        return false;
    }

    if (depth != 1 && type != IMAGE_TYPE_3D) {
        LOGM(WARN, "image depth is not set to one on two dimensional image");
        depth = 1;
    }

    VkImageCreateInfo image_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = image_type,
        .extent = (VkExtent3D){width, height, depth},
        .mipLevels = 1,
        .arrayLayers = (type == IMAGE_TYPE_CUBE_MAP) ? 6 : 1,
        .format = fmt,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = usage,
        .flags = (type == IMAGE_TYPE_CUBE_MAP)
                     ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
                     : 0,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    image->type = type;

    VmaAllocationCreateInfo alloc_info = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };

    if (vmaCreateImage(init->allocator, &image_create_info, &alloc_info,
                       &image->handle, &image->alloc, NULL) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create vulkan image");
        return false;
    }

    VkImageViewCreateInfo view_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image->handle,
        .viewType = view_type,
        .format = fmt,
        .subresourceRange =
            {
                .aspectMask = fmt == VK_FORMAT_D32_SFLOAT
                                  ? VK_IMAGE_ASPECT_DEPTH_BIT
                                  : VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = (type == IMAGE_TYPE_CUBE_MAP) ? 6 : 1,
                .levelCount = 1,
            },
    };

    if (vkCreateImageView(init->dev, &view_create_info, NULL, &image->view) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to create vulkan image");
        return false;
    }

    return true;
}

static vec3 cube_map_face_to_xyz(int x, int y, int face, int size) {
    float u = 2.0f * ((x + 0.5f) / size) - 1.0f;
    float v = 2.0f * ((y + 0.5f) / size) - 1.0f;

    switch (face) {
    case CUBE_MAP_X_POS:
        return (vec3){1.0f, -v, -u};
    case CUBE_MAP_X_NEG:
        return (vec3){-1.0f, -v, u};
    case CUBE_MAP_Y_POS:
        return (vec3){u, 1.0f, v};
    case CUBE_MAP_Y_NEG:
        return (vec3){u, -1.0f, -v};
    case CUBE_MAP_Z_POS:
        return (vec3){u, -v, 1.0f};
    case CUBE_MAP_Z_NEG:
        return (vec3){-u, -v, -1.0f};
    default:
        LOGM(ERROR, "invalid face");
        return (vec3){0};
    }
}

bool vk_image_create_cube_map(struct vk_init *init, struct vk_image *image,
                              const char *path) {
    i32 width, height, bpp;

    // for high precision load as float
    const f32 *data = stbi_loadf(path, &width, &height, &bpp, 4);
    if (!data) {
        LOGM(ERROR, "failed to load image");
        return false;
    }

    if (width != 2 * height) {
        LOGM(ERROR, "expected equirectangular map (2:1 aspect) but it is %dx%d",
             width, height);
        stbi_image_free((void *)data);
        return false;
    }

    i32 face_size = width * 0.25f;
    if (!vk_image_create(
            init, image, face_size, face_size, 1, VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            IMAGE_TYPE_CUBE_MAP)) {
        stbi_image_free((void *)data);
        return false; // error message already printed in create_image
                      // function
    }

    u32 size = face_size * face_size * 4 * sizeof(f32);
    f32 *cube_face_data = malloc(size);
    if (!cube_face_data) {
        LOGM(ERROR, "failed to allocate cube face data");
        return false;
    }

    // for every face
    for (i32 face = 0; face < 6; face++) {
        for (i32 y = 0; y < face_size; y++) {
            for (i32 x = 0; x < face_size; x++) {
                vec3 p =
                    math_vec3_norm(cube_map_face_to_xyz(x, y, face, face_size));

                // convert to 3D spherical coordinates
                f32 r = sqrtf(p.x * p.x + p.z * p.z);
                f32 phi = atan2f(p.z, p.x);
                f32 theta = atan2f(-p.y, r);

                // scaled uv
                f32 u = (f32)((phi + M_PI) / (2.0f * M_PI)) * width;
                f32 v = (f32)((M_PI / 2.0f - theta) / M_PI) * height;

                i32 u1 = math_clampi((i32)roundf(u), 0, width - 1);
                i32 v1 = math_clampi((i32)roundf(v), 0, height - 1);

                v1 = height - 1 - v1;

                const f32 *src = data + (v1 * width + u1) * 4;
                i32 dst_idx = (y * face_size + x) * 4;

                cube_face_data[dst_idx + 0] = src[0];
                cube_face_data[dst_idx + 1] = src[1];
                cube_face_data[dst_idx + 2] = src[2];
                cube_face_data[dst_idx + 3] = src[3];
            }
        }
        vk_image_upload_data(init, image, size, cube_face_data, face_size,
                             face_size, 1,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             VK_ACCESS_SHADER_READ_BIT, &face);
    }

    free(cube_face_data);
    stbi_image_free((void *)data);

    return true;
}

bool vk_image_create_noise(struct vk_init *init, struct vk_image *image) {
    fnl_state noise = fnlCreateState();
    noise.noise_type = FNL_NOISE_PERLIN;
    noise.frequency = 0.25f;

    const u32 noise_size = 64;

    u32 data_size = noise_size * noise_size * noise_size * sizeof(f32);
    f32 *data = malloc(data_size);
    i32 index = 0;

    for (u32 z = 0; z < noise_size; z++) {
        for (u32 y = 0; y < noise_size; y++) {
            for (u32 x = 0; x < noise_size; x++) {
                data[index++] = fnlGetNoise3D(&noise, x, y, z) * 0.5f + 0.5f;
            }
        }
    }

    vk_image_create(
        init, image, noise_size, noise_size, noise_size, VK_FORMAT_R32_SFLOAT,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        IMAGE_TYPE_3D);
    vk_image_upload_data(init, image, data_size, data, noise_size, noise_size,
                         noise_size, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_ACCESS_SHADER_READ_BIT, NULL);

    free(data);

    return true;
}

bool vk_image_transition(struct vk_init *init, struct vk_image *image,
                         VkImageLayout old_layout, VkImageLayout new_layout,
                         VkAccessFlags src_access, VkAccessFlags dst_access,
                         VkPipelineStageFlags src_stage,
                         VkPipelineStageFlags dst_stage,
                         VkImageAspectFlags aspect_mask) {
    VkCommandPool cmd_pool;
    VkCommandBuffer cmd_buffer;

    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = init->graphics_queue.index,
    };

    if (vkCreateCommandPool(init->dev, &pool_info, NULL, &cmd_pool) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to create command pool");
        return false;
    }

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
        .commandPool = cmd_pool,
    };

    if (vkAllocateCommandBuffers(init->dev, &alloc_info, &cmd_buffer) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to create command buffer");
        goto error_path;
    }

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    if (vkBeginCommandBuffer(cmd_buffer, &begin_info) != VK_SUCCESS) {
        LOGM(ERROR, "failed to begin recording into command buffer");
        goto error_path;
    }

    VkImageMemoryBarrier image_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image->handle,
        .subresourceRange =
            {
                .aspectMask = aspect_mask,
                .layerCount = 1,
                .levelCount = 1,
            },
        .srcAccessMask = src_access,
        .dstAccessMask = dst_access,
    };

    vkCmdPipelineBarrier(cmd_buffer, src_stage, dst_stage, 0, 0, 0, 0, 0, 1,
                         &image_barrier);

    if (vkEndCommandBuffer(cmd_buffer) != VK_SUCCESS) {
        LOGM(ERROR, "failed recording into command buffer");
        goto error_path;
    }

    VkSubmitInfo sub_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_buffer,
    };

    if (vkQueueSubmit(init->graphics_queue.handle, 1, &sub_info,
                      VK_NULL_HANDLE) != VK_SUCCESS) {
        LOGM(ERROR, "failed to submit queue");
        goto error_path;
    }

    if (vkQueueWaitIdle(init->graphics_queue.handle) != VK_SUCCESS) {
        LOGM(ERROR, "Failed to wait for queue");
        goto error_path;
    }

    vkDestroyCommandPool(init->dev, cmd_pool, NULL);

    return true;

error_path:

    vkDestroyCommandPool(init->dev, cmd_pool, NULL);
    return false;
}

void vk_image_destroy(struct vk_init *init, struct vk_image *image) {
    vkDestroyImageView(init->dev, image->view, NULL);
    vmaDestroyImage(init->allocator, image->handle, image->alloc);
}

bool vk_image_upload_data(struct vk_init *init, struct vk_image *image,
                          u32 data_size, void *data, u32 width, u32 height,
                          u32 depth, VkImageLayout final_layout,
                          VkAccessFlags dst_access_mask, i32 *cube_face) {
    if (depth != 1 && image->type != IMAGE_TYPE_3D) {
        LOGM(WARN, "image depth is not set to one on two dimensional image");
        depth = 1;
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
        LOGM(ERROR, "failed to create command pool");
        return false;
    }

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
        .commandPool = cmd_pool,
    };

    if (vkAllocateCommandBuffers(init->dev, &alloc_info, &cmd_buffer) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to create command buffer");
        vkDestroyCommandPool(init->dev, cmd_pool, NULL);
        return false;
    }

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    if (vkBeginCommandBuffer(cmd_buffer, &begin_info) != VK_SUCCESS) {
        LOGM(ERROR, "failed to begin recording into command buffer");
        vkDestroyCommandPool(init->dev, cmd_pool, NULL);
        return false;
    }

    VkImageMemoryBarrier image_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image->handle,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
                .levelCount = 1,
                .baseArrayLayer = cube_face ? *cube_face : 0,
            },
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    };

    vkCmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0, 1,
                         &image_barrier);

    struct vk_buffer staging = vk_buffer_create_staging(init, data_size, data);

    VkBufferImageCopy region = {0};
    if (cube_face) {
        if (image->type != IMAGE_TYPE_CUBE_MAP) {
            // TODO: add good error handling
            LOGM(ERROR, "trying to copy cube map into non cube image");
        }

        if (width != height) {
            LOGM(ERROR, "cube map with not equal dimnenstions: %ux%u", width,
                 height);
        }

        i32 face = *cube_face;
        region = (VkBufferImageCopy){
            .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .imageSubresource.baseArrayLayer = face,
            .imageSubresource.layerCount = 1,
            .imageExtent = (VkExtent3D){width, height, depth},
        };
    } else {
        // TODO: add good error handling
        if (image->type == IMAGE_TYPE_CUBE_MAP) {
            LOGM(ERROR, "trying to copy normal image into cube map");
        }

        region = (VkBufferImageCopy){
            .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .imageSubresource.layerCount = 1,
            .imageExtent = (VkExtent3D){width, height, depth},
        };
    }

    vkCmdCopyBufferToImage(cmd_buffer, staging.handle, image->handle,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    image_barrier = (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = final_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image->handle,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
                .levelCount = 1,
                .baseArrayLayer = cube_face ? *cube_face : 0,
            },
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = dst_access_mask,
    };

    vkCmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0,
                         1, &image_barrier);
    if (vkEndCommandBuffer(cmd_buffer) != VK_SUCCESS) {
        LOGM(ERROR, "failed recording into command buffer");
        vmaDestroyBuffer(init->allocator, staging.handle, staging.alloc);
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
        LOGM(ERROR, "failed to submit queue");
        vmaDestroyBuffer(init->allocator, staging.handle, staging.alloc);
        vkDestroyCommandPool(init->dev, cmd_pool, NULL);
        return false;
    }

    if (vkQueueWaitIdle(init->graphics_queue.handle) != VK_SUCCESS) {
        LOGM(ERROR, "Failed to wait for queue");
        vmaDestroyBuffer(init->allocator, staging.handle, staging.alloc);
        vkDestroyCommandPool(init->dev, cmd_pool, NULL);
        return false;
    }

    vkDestroyCommandPool(init->dev, cmd_pool, NULL);

    vmaDestroyBuffer(init->allocator, staging.handle, staging.alloc);

    return true;
}
