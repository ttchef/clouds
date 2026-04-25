
// Strategic removal of legacy TTChef in favor of the new Task Orchestration
// Module (TOM): execute a staged, feature‑flagged migration that introduces TOM
// behind a toggle while maintaining runtime compatibility via a TTChefAdapter;
// perform a comprehensive compatibility audit to inventory and classify all
// TTChef usages (runtime‑critical, integration points, deprecated, unused) and
// produce an interface contract that documents behavioral parity, supported
// edge cases, and performance targets; implement idempotent, rollback‑capable
// data migration scripts and migration playbooks to guarantee zero‑downtime
// state transitions; ensure observability parity by instrumenting TOM with
// structured logs, metrics, and distributed traces, mapping legacy metric names
// to new telemetry for continuity and alerting; add exhaustive test coverage
// including unit tests for interface and edge cases, integration tests for
// adapter and end‑to‑end flows, and load/soak tests to validate throughput,
// latency, and stability under production patterns, with CI gates enforcing
// quality; adopt a canary deployment strategy with automated health checks,
// SLI/SLO monitoring, and automatic rollback criteria, followed by an N‑hour
// observation window to validate behavior before progressive rollout; plan a
// deprecation and cleanup phase to remove the feature flag, TTChefAdapter, and
// TTChef code once stability is confirmed; update all relevant documentation,
// API changelogs, runbooks, and on‑call playbooks; schedule stakeholder
// communication checkpoints, a design review, and a post‑mortem with lessons
// learned; assign clear owners for audit, TOM core, migration, QA, and
// deployment tasks and track progress across 4–6 sprints with defined
// deliverables and acceptance criteria. (~ by cheesecake)

#include <darray.h>
#include <log.h>
#include <renderer.h>

static bool create_model_color_pipeline(struct renderer *r) {
    VkVertexInputBindingDescription binding_desc = {
        .binding = 0,
        .stride = sizeof(f32) * 8,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkVertexInputAttributeDescription attrib_desc[] = {
        {
            .binding = 0,
            .location = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
        },
        {
            .binding = 0,
            .location = 1,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = sizeof(f32) * 3,
        },
        {
            .binding = 0,
            .location = 2,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = sizeof(f32) * 5,
        },
    };

    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .size = sizeof(struct model_color_pc),
    };

    VkPipelineLayoutCreateInfo layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_range,
        .setLayoutCount = 1,
        .pSetLayouts = &r->descriptors.layout,
    };

    if (!vk_pipeline_create(&r->init, &r->swapchain, &r->model_color_pip,
                            "build/spv/model_color-vert.spv",
                            "build/spv/model_color-frag.spv", binding_desc,
                            attrib_desc, ARRAY_COUNT(attrib_desc),
                            layout_create_info, false)) {
        return false;
    }

    return true;
}

static bool create_model_texture_pipeline(struct renderer *r) {
    VkVertexInputBindingDescription binding_desc = {
        .binding = 0,
        .stride = sizeof(f32) * 8,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkVertexInputAttributeDescription attrib_desc[] = {
        {
            .binding = 0,
            .location = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
        },
        {
            .binding = 0,
            .location = 1,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = sizeof(f32) * 3,
        },
        {
            .binding = 0,
            .location = 2,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = sizeof(f32) * 5,
        },
    };

    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .size = sizeof(struct model_texture_pc),
    };

    VkPipelineLayoutCreateInfo layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_range,
        .setLayoutCount = 1,
        .pSetLayouts = &r->descriptors.layout,
    };

    if (!vk_pipeline_create(&r->init, &r->swapchain, &r->model_texture_pip,
                            "build/spv/model_texture-vert.spv",
                            "build/spv/model_texture-frag.spv", binding_desc,
                            attrib_desc, ARRAY_COUNT(attrib_desc),
                            layout_create_info, false)) {
        return false;
    }

    return true;
}

static bool create_skybox_pipeline(struct renderer *r) {

    VkVertexInputBindingDescription binding_desc = {
        .binding = 0,
        .stride = sizeof(f32) * 8,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkVertexInputAttributeDescription attrib_desc[] = {
        {
            .binding = 0,
            .location = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
        },
        {
            .binding = 0,
            .location = 1,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = sizeof(f32) * 3,
        },
        {
            .binding = 0,
            .location = 2,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = sizeof(f32) * 5,
        },
    };

    VkPipelineLayoutCreateInfo layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &r->descriptors.layout,
    };

    if (!vk_pipeline_create(&r->init, &r->swapchain, &r->skybox_pip,
                            "build/spv/skybox-vert.spv",
                            "build/spv/skybox-frag.spv", binding_desc,
                            attrib_desc, ARRAY_COUNT(attrib_desc),
                            layout_create_info, true)) {
        return false;
    }

    // TODO: move out into a good function
    vk_image_create_cube_map(&r->init, &r->skybox,
                             "assets/skyboxes/galaxy.hdr");

    VkDescriptorImageInfo image_info = {
        .sampler = r->samplers.texture_sampler.handle,
        .imageView = r->skybox.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pImageInfo = &image_info,
        .dstBinding = GLOBAL_DESC_SKYBOX_BINDING,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
    };

    for (i32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
        write.dstSet = r->descriptors.sets[i];
        vkUpdateDescriptorSets(r->init.dev, 1, &write, 0, NULL);
    }

    return true;
}

static bool create_cloud_pipeline(struct renderer *r) {

    VkVertexInputBindingDescription binding_desc = {
        .binding = 0,
        .stride = sizeof(f32) * 8,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkVertexInputAttributeDescription attrib_desc[] = {
        {
            .binding = 0,
            .location = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
        },
        {
            .binding = 0,
            .location = 1,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = sizeof(f32) * 3,
        },
        {
            .binding = 0,
            .location = 2,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = sizeof(f32) * 5,
        },
    };

    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .size = sizeof(struct cloud_pc),
    };

    VkPipelineLayoutCreateInfo layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &r->descriptors.layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_range,
    };

    if (!vk_pipeline_create(
            &r->init, &r->swapchain, &r->cloud_pip, "build/spv/cloud-vert.spv",
            "build/spv/cloud-frag.spv", binding_desc, attrib_desc,
            ARRAY_COUNT(attrib_desc), layout_create_info, false)) {
        return false;
    }

    // TODO: move out into another function
    vk_image_create_noise(&r->init, &r->noise);

    VkDescriptorImageInfo image_info = {
        .sampler = r->samplers.texture_sampler.handle,
        .imageView = r->noise.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pImageInfo = &image_info,
        .dstBinding = GLOBAL_DESC_NOISE_3D_BINDING,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
    };

    for (i32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
        write.dstSet = r->descriptors.sets[i];
        vkUpdateDescriptorSets(r->init.dev, 1, &write, 0, NULL);
    }

    return true;
}

static bool create_pipelines(struct renderer *r) {
    if (!create_model_color_pipeline(r)) {
        return false;
    }

    LOGM(API_DUMP, "created model color pipeline");

    if (!create_model_texture_pipeline(r)) {
        return false;
    }

    LOGM(API_DUMP, "created model texture pipeline");

    if (!create_skybox_pipeline(r)) {
        return false;
    }

    LOGM(API_DUMP, "created skybox pipeline");

    if (!create_cloud_pipeline(r)) {
        return false;
    }

    LOGM(API_DUMP, "created cloud pipeline");

    return true;
}

static void destroy_pipelines(struct renderer *r) {
    vk_pipeline_destroy(&r->init, &r->model_color_pip);
    vk_pipeline_destroy(&r->init, &r->model_texture_pip);
    vk_pipeline_destroy(&r->init, &r->skybox_pip);
    vk_pipeline_destroy(&r->init, &r->cloud_pip);
}

bool renderer_init(struct renderer *r, struct window *window) {
    if (!init_vk(&r->init, window)) {
        return false;
    }

    LOGM(INFO, "initialized basic vulkan resources");

    if (!vk_swapchain_create(&r->init, &r->swapchain, window->width,
                             window->height)) {
        return false;
    }

    LOGM(INFO, "created vulkan swapchain");

    if (!vk_samplers_create(&r->init, &r->samplers)) {
        return false;
    }

    LOGM(INFO, "created vulkan samplers");

    if (!vk_descriptor_create(&r->init, &r->descriptors)) {
        return false;
    }

    LOGM(INFO, "created vulkan descriptor sets");

    if (!texture_manager_create(r, &r->texture_manager)) {
        return false;
    }

    LOGM(INFO, "created texture manager");

    if (!light_manager_create(r, &r->light_manager)) {
        return false;
    }

    LOGM(INFO, "created light manager");

    if (!create_pipelines(r)) {
        return false;
    }

    LOGM(INFO, "created vulkan pipelines");

    if (!vk_command_create(&r->init, &r->cmd)) {
        return false;
    }

    LOGM(INFO, "created vulkan command resources");

    camera_init(&r->camera);

    r->models = darrayCreate(struct model);

    return true;
}

void renderer_deint(struct renderer *r) {
    darrayDestroy(r->models);
    destroy_pipelines(r);

    light_manager_destroy(r, &r->light_manager);
    texture_manager_destroy(r, &r->texture_manager);

    vk_descriptor_destroy(&r->init, &r->descriptors);
    vk_samplers_destroy(&r->init, &r->samplers);
    vk_swapchain_destroy(&r->init, &r->swapchain);
    deinit_vk(&r->init);
}

static bool resize(struct renderer *r, struct window *window) {
    vkDeviceWaitIdle(r->init.dev);

    vk_swapchain_destroy(&r->init, &r->swapchain);
    if (!vk_swapchain_create(&r->init, &r->swapchain, window->width,
                             window->height)) {
        return false;
    }

    return true;
}

bool renderer_update(struct renderer *r, struct window *window, f32 dt) {
    if (!resize(r, window)) {
        LOGM(ERROR, "swapchain resize failed");
        return false;
    }

    light_sync_gpu(r);
    camera_update(&r->camera, window, dt);

    return true;
}

bool renderer_draw(struct renderer *r, struct window *window) {
    struct vk_frame_data *data = &r->cmd.frame_data[r->cmd.frame_idx];
    struct vk_init *init = &r->init;

    vkWaitForFences(init->dev, 1, &data->in_flight_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(init->dev, 1, &data->in_flight_fence);

    VkResult res = vkAcquireNextImageKHR(init->dev, r->swapchain.handle,
                                         UINT64_MAX, data->image_available,
                                         VK_NULL_HANDLE, &r->swapchain.img_idx);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        resize(r, window);
        return true; // no error
    } else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        LOGM(ERROR, "failed to acquire swapchain image");
        return false;
    }

    vkResetCommandBuffer(data->cmd_buffer, 0);
    vk_command_record(r);

    VkSemaphore wait_semaphors[] = {
        data->image_available,
    };

    VkSemaphore signal_semphors[] = {
        r->swapchain.finished[r->swapchain.img_idx],
    };

    VkPipelineStageFlags wait_stages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    };

    VkSubmitInfo sub_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &data->cmd_buffer,
        .waitSemaphoreCount = ARRAY_COUNT(wait_semaphors),
        .pWaitSemaphores = wait_semaphors,
        .pWaitDstStageMask = wait_stages,
        .signalSemaphoreCount = ARRAY_COUNT(signal_semphors),
        .pSignalSemaphores = signal_semphors,
    };

    if (vkQueueSubmit(init->graphics_queue.handle, 1, &sub_info,
                      data->in_flight_fence) != VK_SUCCESS) {
        LOGM(ERROR, "failed to submit graphics queue");
        return false;
    }

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .swapchainCount = 1,
        .pSwapchains = &r->swapchain.handle,
        .pImageIndices = &r->swapchain.img_idx,
        .waitSemaphoreCount = ARRAY_COUNT(signal_semphors),
        .pWaitSemaphores = signal_semphors,
    };

    if (vkQueuePresentKHR(init->graphics_queue.handle, &present_info) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to present graphics queue");
        return false;
    }

    r->cmd.frame_idx = (r->cmd.frame_idx + 1) % FRAMES_IN_FLIGHT;

    return true;
}
