
#include <vk/command.h>
#include <vk/init.h>

#include <log.h>

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
