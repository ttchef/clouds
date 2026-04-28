
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

#include "vk/swapchain.h"
#include <darray.h>
#include <full_types.h>
#include <log.h>
#include <renderer.h>
#include <vulkan/vulkan_core.h>

static bool create_pipelines(struct renderer *r) {
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

    struct vk_pipeline_desc desc = vk_pipeline_desc_default();

    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
    };

    // general description matching all pipelines
    vk_pipeline_set_blend_state(&desc, &color_blend_attachment, 1);
    vk_pipeline_set_vertex_input(&desc, &binding_desc, 1, attrib_desc,
                                 ARRAY_COUNT(attrib_desc));
    vk_pipeline_set_descriptor(&desc, &r->descriptors.layout, 1);

    // custom description
    vk_pipeline_set_shaders(&desc, "src/shaders/model_color.vert",
                            "src/shaders/model_color.frag");
    vk_pipeline_set_push_constant(&desc, sizeof(struct model_color_pc),
                                  VK_SHADER_STAGE_VERTEX_BIT |
                                      VK_SHADER_STAGE_FRAGMENT_BIT);

    r->model_color_pip = vk_pipeline_create(&r->init, &r->swapchain,
                                            &r->pipeline_manager, &desc);
    if (r->model_color_pip == NO_PIPELINE) {
        return false;
    }

    LOGM(API_DUMP, "created model color pipeline");

    vk_pipeline_set_shaders(&desc, "src/shaders/model_texture.vert",
                            "src/shaders/model_texture.frag");

    vk_pipeline_set_push_constant(&desc, sizeof(struct model_texture_pc),
                                  VK_SHADER_STAGE_VERTEX_BIT |
                                      VK_SHADER_STAGE_FRAGMENT_BIT);

    r->model_texture_pip = vk_pipeline_create(&r->init, &r->swapchain,
                                              &r->pipeline_manager, &desc);
    if (r->model_texture_pip == NO_PIPELINE) {
        return false;
    }

    LOGM(API_DUMP, "created model texture pipeline");

    vk_pipeline_set_shaders(&desc, "src/shaders/cloud.vert",
                            "src/shaders/cloud.frag");

    vk_pipeline_set_push_constant(&desc, sizeof(struct cloud_pc),
                                  VK_SHADER_STAGE_VERTEX_BIT |
                                      VK_SHADER_STAGE_FRAGMENT_BIT);

    r->cloud_pip = vk_pipeline_create(&r->init, &r->swapchain,
                                      &r->pipeline_manager, &desc);
    if (r->cloud_pip == NO_PIPELINE) {
        return false;
    }

    LOGM(API_DUMP, "created cloud pipeline");

    // TODO: move out into another function
    // cloud noise image
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

    vk_pipeline_set_shaders(&desc, "src/shaders/skybox.vert",
                            "src/shaders/skybox.frag");
    vk_pipeline_set_push_constant(&desc, 0, 0);
    vk_pipeline_set_cull_mode(&desc, VK_CULL_MODE_FRONT_BIT,
                              VK_FRONT_FACE_COUNTER_CLOCKWISE);
    vk_pipeline_set_depth_state(&desc, VK_FALSE, VK_TRUE,
                                VK_COMPARE_OP_LESS_OR_EQUAL);

    r->skybox_pip = vk_pipeline_create(&r->init, &r->swapchain,
                                       &r->pipeline_manager, &desc);
    if (r->skybox_pip == NO_PIPELINE) {
        return false;
    }

    // TODO: move out into a good function
    // skybox cube map
    vk_image_create_cube_map(&r->init, &r->skybox,
                             "assets/skyboxes/planet.hdr");

    image_info = (VkDescriptorImageInfo){
        .sampler = r->samplers.texture_sampler.handle,
        .imageView = r->skybox.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    write = (VkWriteDescriptorSet){
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

    if (!vk_pipeline_manager_create(&r->pipeline_manager)) {
        return false;
    }

    LOGM(INFO, "created pipeline manager");

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

    if (!vk_matrix_ubo_create(r, &r->matrix_ubo)) {
        return false;
    }

    LOGM(INFO, "created matrix uniform buffer");

    draw_init(&r->render_queue);
    camera_init(&r->camera);

    r->models = darrayCreate(struct model);
    model_create_file(r, "assets/models/box.glb");

    return true;
}

void renderer_deint(struct renderer *r) {
    vkDeviceWaitIdle(r->init.dev);

    vk_matrix_ubo_destroy(r, &r->matrix_ubo);
    darrayDestroy(r->models);
    vk_pipeline_manager_destroy(&r->init, &r->pipeline_manager);

    light_manager_destroy(r, &r->light_manager);
    texture_manager_destroy(r, &r->texture_manager);

    vk_descriptor_destroy(&r->init, &r->descriptors);
    vk_samplers_destroy(&r->init, &r->samplers);
    vk_swapchain_destroy(&r->init, &r->swapchain);
    deinit_vk(&r->init);
}

bool renderer_resize(struct renderer *r, u32 width, u32 height) {
    if (!vk_swapchain_recreate(&r->init, &r->swapchain, width, height,
                               r->cmd.frame_idx_not_cleared)) {
        return false;
    }

    return true;
}

bool renderer_update(struct renderer *r, struct window *window, f32 dt) {
    light_sync_gpu(r);
    camera_update(&r->camera, window, dt);
    vk_pipeline_manager_check_reload(&r->init, &r->swapchain,
                                     &r->pipeline_manager);

    f32 aspect =
        (f32)r->swapchain.extent.width / (f32)r->swapchain.extent.height;
    matrix perspective = math_matrix_perspective(50, aspect, 0.1f, 100.0f);
    matrix view = math_matrix_look_at(
        r->camera.pos, math_vec3_add(r->camera.pos, r->camera.direction),
        (vec3){0.0f, 1.0f, 0.0f});

    r->matrix_ubo.data.proj = perspective;
    r->matrix_ubo.data.view = view;

    // doesnt work when i do it in the shader idk why xD
    r->matrix_ubo.data.proj_view = math_matrix_mul(perspective, view);

    vk_matrix_ubo_sync_data(r, &r->matrix_ubo);

    return true;
}

bool renderer_draw(struct renderer *r, struct window *window) {
    struct vk_frame_data *data = &r->cmd.frame_data[r->cmd.frame_idx];
    struct vk_init *init = &r->init;

    vkWaitForFences(init->dev, 1, &data->in_flight_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(init->dev, 1, &data->in_flight_fence);

    vk_swapchain_drain(init, &r->swapchain, r->cmd.frame_idx_not_cleared);

    VkResult res = vkAcquireNextImageKHR(init->dev, r->swapchain.handle,
                                         UINT64_MAX, data->image_available,
                                         VK_NULL_HANDLE, &r->swapchain.img_idx);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        renderer_resize(r, window->width, window->height);
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
    r->cmd.frame_idx_not_cleared++;

    return true;
}
