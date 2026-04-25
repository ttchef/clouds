
#include <full_types.h>
#include <light.h>
#include <log.h>
#include <renderer.h>

#include <vulkan/vulkan.h>

#include <string.h>

#define SHADOW_MAP_SIZE 1024

enum {
    LIGHT_TYPE_DIRECTIONAL,
    LIGHT_TYPE_POINT,
    LIGHT_TYPE_SPOT,
};

// TODO: improve pipeline builder to support this special kind of pipeline
static bool create_shadow_pipeline(struct renderer *r,
                                   struct vk_pipeline *pipeline) {

    VkShaderModule vert_module;
    if (!vk_shader_module_create(&r->init, &vert_module,
                                 "build/spv/shadow-vert.spv")) {
        return false;
    }

    LOGM(API_DUMP, "created vertex shader module");

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_module,
            .pName = "main",
        },
    };

    VkPipelineRenderingCreateInfo dynamic_rendering = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 0,
        .pColorAttachmentFormats = NULL,
        .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
    };

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

    VkPipelineVertexInputStateCreateInfo vertex = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_desc,
        .vertexAttributeDescriptionCount = ARRAY_COUNT(attrib_desc),
        .pVertexAttributeDescriptions = attrib_desc,
    };

    VkPipelineInputAssemblyStateCreateInfo assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkPipelineViewportStateCreateInfo viewport = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterization = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .lineWidth = 1.0f,
        .depthClampEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_FRONT_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_TRUE,
    };

    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };

    VkDynamicState dym_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dym_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = ARRAY_COUNT(dym_states),
        .pDynamicStates = dym_states,
    };

    VkPipelineColorBlendStateCreateInfo color_blend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    };

    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .size = sizeof(struct shadow_pc),
    };

    VkPipelineLayoutCreateInfo layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_range,
        .setLayoutCount = 1,
        .pSetLayouts = &r->descriptors.layout,
    };

    if (vkCreatePipelineLayout(r->init.dev, &layout_create_info, NULL,
                               &pipeline->layout) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create pipeline layout");
        goto error_path;
    }

    VkGraphicsPipelineCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &dynamic_rendering,
        .layout = pipeline->layout,
        .stageCount = ARRAY_COUNT(shader_stages),
        .pStages = shader_stages,
        .pVertexInputState = &vertex,
        .pInputAssemblyState = &assembly,
        .pViewportState = &viewport,
        .pRasterizationState = &rasterization,
        .pMultisampleState = &multisample,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blend,
        .pDynamicState = &dym_state,
        .renderPass = VK_NULL_HANDLE, // better safe than sorry xD
    };

    if (vkCreateGraphicsPipelines(r->init.dev, 0, 1, &create_info, NULL,
                                  &pipeline->handle) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create shadow pipeline");
        goto error_path;
    }

    vkDestroyShaderModule(r->init.dev, vert_module, NULL);

    return true;

error_path:
    vkDestroyShaderModule(r->init.dev, vert_module, NULL);

    return false;
}

bool light_manager_create(struct renderer *r, struct light_manager *manager) {
    VkDescriptorBufferInfo buffer_info = {
        .offset = 0,
        .range = sizeof(struct light_buffer),
    };

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = GLOBAL_DESC_LIGHT_BINDING,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &buffer_info,
    };

    for (i32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
        manager->buffers[i] =
            vk_buffer_create_host_visible(&r->init, sizeof(struct light_buffer),
                                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        buffer_info.buffer = manager->buffers[i].handle;
        write.dstSet = r->descriptors.sets[i];

        vkUpdateDescriptorSets(r->init.dev, 1, &write, 0, NULL);
    }

    create_shadow_pipeline(r, &manager->shadow_pip);

    return true;
}

void light_manager_destroy(struct renderer *r, struct light_manager *manager) {
    struct vk_init *init = &r->init;

    for (i32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vmaDestroyBuffer(init->allocator, manager->buffers[i].handle,
                         manager->buffers[i].alloc);
    }
}

static u32 create_shadow_map(struct renderer *r, struct vk_image *image,
                             u32 binding, u32 *counter) {
    vk_image_create(&r->init, image, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 1,
                    VK_FORMAT_D32_SFLOAT,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                        VK_IMAGE_USAGE_SAMPLED_BIT,
                    IMAGE_TYPE_2D);

    VkDescriptorImageInfo image_info = {
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .imageView = image->view,
        .sampler = r->samplers.texture_sampler.handle,
    };

    u32 index = *counter;

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = binding,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .dstArrayElement = index,
        .descriptorCount = 1,
        .pImageInfo = &image_info,
    };

    (*counter)++;

    for (i32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
        write.dstSet = r->descriptors.sets[i];
        vkUpdateDescriptorSets(r->init.dev, 1, &write, 0, NULL);
    }

    vk_image_transition(
        &r->init, image, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

    return index;
}

static light_id create_light_id(i32 type, i32 index) {
    switch (type) {
    case LIGHT_TYPE_DIRECTIONAL:
        return index;
    case LIGHT_TYPE_POINT:
        return MAX_DIRECTIONAL_LIGHTS + index;
    case LIGHT_TYPE_SPOT:
        return MAX_DIRECTIONAL_LIGHTS + MAX_POINT_LIGHTS + index;
    }

    LOGM(WARN, "failed to create light");
    return NO_LIGHT;
}

light_id light_dir_create(struct renderer *r, vec3 direction, vec3 color) {
    struct light_manager *m = &r->light_manager;

    struct dir_light res = {
        .direction = direction,
        .color = color,
        .valid = true,
    };

    i32 i = 0;
    for (; i < MAX_DIRECTIONAL_LIGHTS; i++) {
        if (!m->directional[i].valid) {
            break;
        }
    }

    if (i == MAX_DIRECTIONAL_LIGHTS) {
        LOGM(WARN, "reached maximum number of directional lights");
        return NO_LIGHT;
    }

    // light_space
    vec3 light_pos = (vec3){0, 10, 10};
    vec3 target = math_vec3_add(light_pos,
                                res.direction); // TODO: change to actual target
    vec3 up = (vec3){0, 1, 0};

    matrix light_view = math_matrix_look_at(light_pos, target, up);
    matrix ortho = math_matrix_orthographic(-20, 20, -20, 20, 0.1f, 100.0f);

    res.transform = math_matrix_mul(ortho, light_view);

    res.shadow_index =
        create_shadow_map(r, &res.map, GLOBAL_DESC_SHADOW_DIRECTIONAL_BINDING,
                          &m->directional_counter);

    m->directional[i] = res;

    return create_light_id(LIGHT_TYPE_DIRECTIONAL, i);
}

static void get_light_distance_coeffecients(f32 distance, f32 *kc, f32 *kl,
                                            f32 *kq) {
    // TODO: check formula if values give good results
    *kc = 1;
    *kl = 3 / distance;
    *kq = 6 / distance;
}

light_id light_point_create(struct renderer *r, vec3 pos, vec3 color,
                            f32 distance) {
    struct light_manager *m = &r->light_manager;

    f32 kc, kl, kq;
    get_light_distance_coeffecients(distance, &kc, &kl, &kq);
    struct point_light res = {
        .pos = pos,
        .color = color,
        .valid = true,

        .constant = kc,
        .linear = kl,
        .quadratic = kq,
    };

    i32 i = 0;
    for (; i < MAX_POINT_LIGHTS; i++) {
        if (!m->point[i].valid) {
            break;
        }
    }

    if (i == MAX_POINT_LIGHTS) {
        LOGM(WARN, "reached maximum number of point lights");
        return NO_LIGHT;
    }

    m->point[i] = res;

    return create_light_id(LIGHT_TYPE_POINT, i);
}

light_id light_spot_create(struct renderer *r, vec3 pos, vec3 direction,
                           vec3 color, f32 distance, f32 cut_of,
                           f32 outer_cut_off) {
    struct light_manager *m = &r->light_manager;

    f32 kc, kl, kq;
    get_light_distance_coeffecients(distance, &kc, &kl, &kq);
    struct spot_light res = {
        .pos = pos,
        .direction = direction,
        .color = color,
        .cutt_off = cos(DEG2RAD(cut_of)),
        .outer_cutt_off = cos(DEG2RAD(outer_cut_off)),
        .valid = true,

        .constant = kc,
        .linear = kl,
        .quadratic = kq,
    };

    i32 i = 0;
    for (; i < MAX_SPOT_LIGHTS; i++) {
        if (!m->spot[i].valid) {
            break;
        }
    }

    if (i == MAX_SPOT_LIGHTS) {
        LOGM(WARN, "reached maximum number of spot lights");
        return NO_LIGHT;
    }

    // light_space
    vec3 dir_n = math_vec3_norm(res.direction);
    vec3 up = (fabsf(dir_n.y) > 0.99f) ? (vec3){1, 0, 0} : (vec3){0, 1, 0};
    vec3 target = math_vec3_add(res.pos, dir_n);

    matrix light_view = math_matrix_look_at(res.pos, target, up);
    matrix proj =
        math_matrix_perspective(outer_cut_off * 2.0f, 1.0f, 0.1f, distance);

    res.transform = math_matrix_mul(proj, light_view);

    res.shadow_index = create_shadow_map(
        r, &res.map, GLOBAL_DESC_SHADOW_SPOT_BINDING, &m->spot_counter);

    m->spot[i] = res;

    return create_light_id(LIGHT_TYPE_SPOT, i);
}

void light_destroy(struct renderer *r, light_id id) {
    if (id > (i32)MAX_DIRECTIONAL_LIGHTS + MAX_POINT_LIGHTS + MAX_SPOT_LIGHTS ||
        id < 0) {
        LOGM(ERROR, "invalid index");
        return;
    }

    struct light_manager *m = &r->light_manager;

    // get real id
    if (id >= MAX_DIRECTIONAL_LIGHTS + MAX_POINT_LIGHTS) {
        id -= MAX_DIRECTIONAL_LIGHTS + MAX_POINT_LIGHTS;

        m->spot[id].valid = false;
        vk_image_destroy(&r->init, &m->spot[id].map);
    } else if (id >= MAX_DIRECTIONAL_LIGHTS) {
        id -= MAX_DIRECTIONAL_LIGHTS;

        m->point[id].valid = false;
        vk_image_destroy(&r->init, &m->point[id].map);
    } else {
        m->directional[id].valid = false;
        vk_image_destroy(&r->init, &m->directional[id].map);
    }
}

void light_state_set(struct renderer *r, light_id id, bool on) {
    if (id < 0 ||
        id > MAX_DIRECTIONAL_LIGHTS + MAX_POINT_LIGHTS + MAX_SPOT_LIGHTS) {
        LOGM(ERROR, "invalid index");
        return;
    }

    struct light_manager *m = &r->light_manager;

    // get real id
    if (id >= MAX_DIRECTIONAL_LIGHTS + MAX_POINT_LIGHTS) {
        id -= MAX_DIRECTIONAL_LIGHTS + MAX_POINT_LIGHTS;

        m->spot[id].valid = on;
    } else if (id >= MAX_DIRECTIONAL_LIGHTS) {
        id -= MAX_DIRECTIONAL_LIGHTS;

        m->point[id].valid = on;
    } else {
        m->directional[id].valid = on;
    }
}

void light_dir_update(struct renderer *r, light_id id, vec3 direction,
                      vec3 color) {
    if (id < 0 || id > MAX_DIRECTIONAL_LIGHTS) {
        LOGM(ERROR, "invalid index");
        return;
    }

    struct light_manager *m = &r->light_manager;

    m->directional[id].direction = direction;
    m->directional[id].color = color;
}

void light_point_update(struct renderer *r, light_id id, vec3 pos, vec3 color,
                        f32 distance) {
    if (id < MAX_DIRECTIONAL_LIGHTS ||
        id > MAX_DIRECTIONAL_LIGHTS + MAX_POINT_LIGHTS) {
        LOGM(ERROR, "invalid index");
        return;
    }

    struct light_manager *m = &r->light_manager;

    f32 kc, kl, kq;
    get_light_distance_coeffecients(distance, &kc, &kl, &kq);

    id -= MAX_DIRECTIONAL_LIGHTS;

    m->point[id].pos = pos;
    m->point[id].color = color;
    m->point[id].constant = kc;
    m->point[id].linear = kl;
    m->point[id].quadratic = kq;
}

void light_spot_update(struct renderer *r, light_id id, vec3 pos,
                       vec3 direction, vec3 color, f32 distance, f32 cutt_of,
                       f32 outer_cutt_of) {
    if (id < MAX_DIRECTIONAL_LIGHTS + MAX_POINT_LIGHTS ||
        id > MAX_DIRECTIONAL_LIGHTS + MAX_POINT_LIGHTS + MAX_SPOT_LIGHTS) {
        LOGM(ERROR, "invalid index");
        return;
    }

    struct light_manager *m = &r->light_manager;

    f32 kc, kl, kq;
    get_light_distance_coeffecients(distance, &kc, &kl, &kq);

    id -= MAX_DIRECTIONAL_LIGHTS + MAX_POINT_LIGHTS;

    struct spot_light *l = &m->spot[id];

    l->pos = pos;
    l->direction = direction;
    l->color = color;
    l->cutt_off = cos(DEG2RAD(cutt_of));
    l->outer_cutt_off = cos(DEG2RAD(outer_cutt_of));

    l->constant = kc;
    l->linear = kl;
    l->quadratic = kq;

    // light_space
    vec3 dir_n = math_vec3_norm(l->direction);
    vec3 up = (fabsf(dir_n.y) > 0.99f) ? (vec3){1, 0, 0} : (vec3){0, 1, 0};
    vec3 target = math_vec3_add(l->pos, dir_n);

    matrix light_view = math_matrix_look_at(l->pos, target, up);
    // matrix proj =
    // math_matrix_perspective(outer_cutt_of * 2.0f, 1.0f, 0.1f, distance);

    // proj doesnt work for some unkwon reason
    matrix ortho = math_matrix_orthographic(-10, 10, -10, 10, 0.1f, distance);

    l->transform = math_matrix_mul(ortho, light_view);
}

bool light_sync_gpu(struct renderer *r) {
    struct light_manager *m = &r->light_manager;

    struct light_buffer *gpu_lights =
        m->buffers[r->cmd.frame_idx].host_visible.mapped;

    m->light_buffer.directional_count = 0;
    m->light_buffer.point_count = 0;
    m->light_buffer.spot_count = 0;

    for (u32 i = 0; i < MAX_DIRECTIONAL_LIGHTS; i++) {
        if (!m->directional[i].valid) {
            continue;
        }

        struct dir_light *light = &m->directional[i];

        struct gpu_dir_light gpu_dir_light = {
            .direction = math_vec4_from_vec3(light->direction, 0.0f),
            .color = math_vec4_from_vec3(light->color, 1.0f),
            .transform = light->transform,
            .shadow_index = light->shadow_index,
        };

        m->light_buffer.directional[m->light_buffer.directional_count++] =
            gpu_dir_light;
    }

    for (u32 i = 0; i < MAX_POINT_LIGHTS; i++) {
        if (!m->point[i].valid) {
            continue;
        }

        struct point_light *light = &m->point[i];

        struct gpu_point_light gpu_point_light = {
            .pos = math_vec4_from_vec3(light->pos, 1.0f),
            .color = math_vec4_from_vec3(light->color, 1.0f),
            .attenuation =
                (vec4){
                    light->constant,
                    light->linear,
                    light->quadratic,
                    0.0f,
                },
        };

        m->light_buffer.point[m->light_buffer.point_count++] = gpu_point_light;
    }

    for (u32 i = 0; i < MAX_SPOT_LIGHTS; i++) {
        if (!m->spot[i].valid) {
            continue;
        }

        struct spot_light *light = &m->spot[i];

        struct gpu_spot_light gpu_spot_light = {
            .pos = math_vec4_from_vec3(light->pos, 1.0f),
            .direction = math_vec4_from_vec3(light->direction, 0.0f),
            .color = math_vec4_from_vec3(light->color, 1.0f),
            .cut_offs =
                (vec4){
                    light->cutt_off,
                    light->outer_cutt_off,
                    0.0f,
                    0.0f,
                },
            .attenuation =
                (vec4){
                    light->constant,
                    light->linear,
                    light->quadratic,
                    0.0f,
                },
            .shadow_index = light->shadow_index,
            .transform = light->transform,
        };

        m->light_buffer.spot[m->light_buffer.spot_count++] = gpu_spot_light;
    }

    memcpy(gpu_lights, &m->light_buffer, sizeof(struct light_buffer));
    return true;
}
