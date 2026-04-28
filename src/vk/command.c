
#include <vk/command.h>
#include <vk/init.h>

#include <draw.h>
#include <full_types.h>
#include <log.h>
#include <renderer.h>

bool vk_command_create(struct vk_init *init, struct vk_command *cmd) {
    cmd->frame_idx = 0;

    VkCommandPoolCreateInfo pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = init->graphics_queue.index,
    };

    if (vkCreateCommandPool(init->dev, &pool_create_info, NULL,
                            &cmd->cmd_pool) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create command pool");
        return false;
    }

    LOGM(API_DUMP, "created command pool");

    VkSemaphoreCreateInfo sem_create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkFenceCreateInfo fence_create_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (u32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
        VkCommandBufferAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = cmd->cmd_pool,
            .commandBufferCount = 1,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        };

        if (vkAllocateCommandBuffers(init->dev, &alloc_info,
                                     &cmd->frame_data[i].cmd_buffer) !=
            VK_SUCCESS) {
            LOGM(ERROR, "failed to allocate command buffer: %d", i);
            return false;
        }

        LOGM(API_DUMP, "created command buffer: %d", i);

        if (vkCreateSemaphore(init->dev, &sem_create_info, NULL,
                              &cmd->frame_data[i].image_available) !=
            VK_SUCCESS) {
            LOGM(ERROR, "failed to create image availabe semaphore: %d", i);
            return false;
        }

        if (vkCreateFence(init->dev, &fence_create_info, NULL,
                          &cmd->frame_data[i].in_flight_fence) != VK_SUCCESS) {
            LOGM(ERROR, "failed to create fence: %d", i);
            return false;
        }
    }

    return true;
}

void vk_command_destroy(struct vk_init *init, struct vk_command *cmd) {
    for (u32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(init->dev, cmd->frame_data[i].image_available, NULL);
        vkDestroyFence(init->dev, cmd->frame_data[i].in_flight_fence, NULL);
    }

    vkDestroyCommandPool(init->dev, cmd->cmd_pool, NULL);
}

static void record_skybox(struct renderer *r, struct vk_frame_data *data) {
    VkDeviceSize offsets[] = {0};

    struct vk_pipeline *skybox_pip =
        vk_pipeline_manager_get(&r->pipeline_manager, r->skybox_pip);

    vkCmdBindPipeline(data->cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      skybox_pip->handle);

    vkCmdBindVertexBuffers(data->cmd_buffer, 0, 1,
                           &r->models[r->box_id].vertex_buffer.handle, offsets);
    vkCmdBindIndexBuffer(data->cmd_buffer,
                         r->models[r->box_id].index_buffer.handle, 0,
                         VK_INDEX_TYPE_UINT16);

    vkCmdBindDescriptorSets(data->cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            skybox_pip->layout, 0, 1,
                            &r->descriptors.sets[r->cmd.frame_idx], 0, NULL);

    vkCmdDrawIndexed(data->cmd_buffer, r->models[r->box_id].n_index, 1, 0, 0,
                     0);
}

static void record_shadow_map(struct renderer *r, struct vk_frame_data *data,
                              struct vk_image *map, matrix transform) {
    VkImageMemoryBarrier shadow_mem_barrier = (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = map->handle,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .layerCount = 1,
                .levelCount = 1,
            },
        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    vkCmdPipelineBarrier(data->cmd_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, NULL,
                         0, NULL, 1, &shadow_mem_barrier);

    VkRenderingAttachmentInfo shadow_depth_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = map->view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue =
            {
                .depthStencil = {1.0f, 0.0f},
            },
    };

    VkRenderingInfo shadow_render_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .layerCount = 1,
        .colorAttachmentCount = 0,
        .pColorAttachments = NULL,
        .pDepthAttachment = &shadow_depth_attachment_info,
        .renderArea =
            {
                .extent = (VkExtent2D){1024, 1024},
                .offset = (VkOffset2D){0, 0},
            },
    };

    vkCmdBeginRendering(data->cmd_buffer, &shadow_render_info);

    VkViewport viewport1 = {
        .x = 0,
        .y = 0,
        .width = 1024,
        .height = 1024,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRect2D scissor1 = {
        .extent = (VkExtent2D){1024, 1024},
        .offset = (VkOffset2D){0, 0},
    };

    vkCmdSetViewport(data->cmd_buffer, 0, 1, &viewport1);
    vkCmdSetScissor(data->cmd_buffer, 0, 1, &scissor1);

    struct shadow_pc push_constant = {
        .light_space = transform,
    };

    draw_cmds(r, data, true, &push_constant);

    vkCmdEndRendering(data->cmd_buffer);

    shadow_mem_barrier = (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .image = map->handle,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .layerCount = 1,
                .levelCount = 1,
            },
        .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
    };

    vkCmdPipelineBarrier(data->cmd_buffer,
                         VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                         NULL, 1, &shadow_mem_barrier);
}

bool vk_command_record(struct renderer *r) {
    struct vk_frame_data *data = &r->cmd.frame_data[r->cmd.frame_idx];

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };

    if (vkBeginCommandBuffer(data->cmd_buffer, &begin_info) != VK_SUCCESS) {
        LOGM(ERROR, "failed to begin recording in the command buffer: %d",
             r->cmd.frame_idx);
        return false;
    }

    for (i32 i = 0; i < MAX_DIRECTIONAL_LIGHTS; i++) {
        if (!r->light_manager.directional[i].valid) {
            continue;
        }

        struct dir_light *l = &r->light_manager.directional[i];
        record_shadow_map(r, data, &l->map, l->transform);
    }

    for (i32 i = 0; i < MAX_SPOT_LIGHTS; i++) {
        if (!r->light_manager.spot[i].valid) {
            continue;
        }

        struct spot_light *l = &r->light_manager.spot[i];
        record_shadow_map(r, data, &l->map, l->transform);
    }

    VkImageMemoryBarrier mem_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = r->swapchain.imgs[r->swapchain.img_idx],
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
                .levelCount = 1,
            },
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    vkCmdPipelineBarrier(data->cmd_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                         NULL, 0, NULL, 1, &mem_barrier);

    mem_barrier = (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = r->swapchain.depth_images[r->swapchain.img_idx].handle,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .layerCount = 1,
                .levelCount = 1,
            },
        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    vkCmdPipelineBarrier(data->cmd_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, NULL,
                         0, NULL, 1, &mem_barrier);

    VkViewport viewport = {
        .width = r->swapchain.extent.width,
        .height = r->swapchain.extent.height,
        .maxDepth = 1.0f,
    };

    VkRect2D scissor = {
        .extent = r->swapchain.extent,
        .offset = (VkOffset2D){0, 0},
    };

    vkCmdSetViewport(data->cmd_buffer, 0, 1, &viewport);
    vkCmdSetScissor(data->cmd_buffer, 0, 1, &scissor);

    VkClearValue clear_color = {
        .color = {{0.0f, 0.0f, 0.0f, 0.0f}},
    };

    VkRenderingAttachmentInfo color_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = r->swapchain.imgs_views[r->swapchain.img_idx],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clear_color,
    };

    VkRenderingAttachmentInfo depth_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = r->swapchain.depth_images[r->swapchain.img_idx].view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue =
            {
                .depthStencil = {1.0f, 0.0f},
            },
    };

    VkRenderingInfo render_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_info,
        .pDepthAttachment = &depth_attachment_info,
        .renderArea =
            {
                .extent = r->swapchain.extent,
                .offset = (VkOffset2D){0, 0},
            },
    };

    vkCmdBeginRendering(data->cmd_buffer, &render_info);

    record_skybox(r, data);
    draw_cmds(r, data, false, NULL);

    vkCmdEndRendering(data->cmd_buffer);

    mem_barrier = (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = r->swapchain.imgs[r->swapchain.img_idx],
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
                .levelCount = 1,
            },
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    vkCmdPipelineBarrier(data->cmd_buffer,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0,
                         NULL, 1, &mem_barrier);

    if (vkEndCommandBuffer(data->cmd_buffer) != VK_SUCCESS) {
        LOGM(ERROR, "failed to end recording in the command buffer: %d",
             r->cmd.frame_idx);
        return false;
    }

    return true;
}
