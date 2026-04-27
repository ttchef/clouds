
#include <vk/init.h>
#include <vk/pipeline.h>
#include <vk/swapchain.h>

#include <log.h>

#include <string.h>
#include <sys/stat.h>

#include <vulkan/vulkan_core.h>

bool vk_shader_module_create(struct vk_init *init, VkShaderModule *module,
                             const char *filename) {
    FILE *shader_fd = fopen(filename, "rb");
    if (!shader_fd) {
        LOGM(ERROR, "failed to open shader file: %s", filename);
        return false;
    }

    fseek(shader_fd, 0, SEEK_END);
    i64 shader_size = ftell(shader_fd);
    rewind(shader_fd);

    if ((shader_size & 0x03) != 0) {
        LOGM(ERROR, "shader compile is not a multiple of 4");
        fclose(shader_fd);
        return false;
    }

    // doesnt need to be null terminated because vulkan takes it len based
    // atleast thats what i think
    u8 shader_str[shader_size];
    fread(shader_str, 1, shader_size, shader_fd);
    fclose(shader_fd);

    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shader_size,
        .pCode = (u32 *)shader_str,
    };

    if (vkCreateShaderModule(init->dev, &create_info, NULL, module) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to create shader module: %s", filename);
        return false;
    }

    return true;
}

vk_pipeline_id vk_pipeline_create(struct vk_init *init,
                                  struct vk_swapchain *swapchain,
                                  struct vk_pipeline_manager *manager,
                                  struct vk_pipeline_desc *desc) {
    // check if space for the pipeline
    if (manager->count >= MAX_PIPELINES) {
        LOGM(ERROR, "not enough space for pipeline increase MAX_PIPELINES");
        return NO_PIPELINE;
    }

    // max of 2 stages (dont need compute at the moment)
    VkPipelineShaderStageCreateInfo shader_stages[2];

    // for handling the destroying of the modules
    VkShaderModule shader_modules[2];
    u32 shader_stage_index = 0;

    if (desc->vert_path) {
        if (!vk_shader_module_create(init, &shader_modules[shader_stage_index],
                                     desc->vert_path)) {
            return NO_PIPELINE;
        }

        shader_stages[shader_stage_index] = (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = shader_modules[shader_stage_index],
            .pName = "main",
        };

        shader_stage_index++;
    }

    if (desc->frag_path) {
        if (!vk_shader_module_create(init, &shader_modules[shader_stage_index],
                                     desc->frag_path)) {
            return NO_PIPELINE;
        }

        shader_stages[shader_stage_index] = (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = shader_modules[shader_stage_index],
            .pName = "main",
        };

        shader_stage_index++;
    }

    VkPipelineRenderingCreateInfo dynamic_rendering = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapchain->fmt,
        .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
    };

    VkPipelineVertexInputStateCreateInfo vertex = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = desc->binding_count,
        .pVertexBindingDescriptions = desc->bindings,
        .vertexAttributeDescriptionCount = desc->attribute_count,
        .pVertexAttributeDescriptions = desc->attributes,
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
        .polygonMode = desc->polygon_mode,
        .cullMode = desc->cull_mode,
        .frontFace = desc->cull_mode,
        .depthBiasEnable = VK_FALSE,
    };

    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = desc->depth_test,
        .depthWriteEnable = desc->depth_write,
        .depthCompareOp = desc->depth_compare_op,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };

    VkPipelineColorBlendStateCreateInfo color_blend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = desc->blend_attachment_count,
        .pAttachments = desc->blend_attachment,
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

    VkPushConstantRange push_constant_range = {
        .offset = 0,
        .size = desc->push_constant_size,
        .stageFlags = desc->push_constant_stages,
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = desc->descriptor_set_layout_count,
        .pSetLayouts = desc->descriptor_set_layouts,
        .pushConstantRangeCount = desc->push_constant_size == 0 ? 0 : 1,
        .pPushConstantRanges = &push_constant_range,
    };

    struct vk_pipeline *pip = &manager->entries[manager->count++];

    if (vkCreatePipelineLayout(init->dev, &pipeline_layout_create_info, NULL,
                               &pip->layout) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create pipeline layout");
        goto error_path;
    }

    VkGraphicsPipelineCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &dynamic_rendering,
        .layout = pip->layout,
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

    if (vkCreateGraphicsPipelines(init->dev, 0, 1, &create_info, NULL,
                                  &pip->handle) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create graphics pipeline");
        goto error_path;
    }

    for (u32 i = 0; i < shader_stage_index; i++) {
        vkDestroyShaderModule(init->dev, shader_modules[i], NULL);
    }

    return true;

error_path:
    for (u32 i = 0; i < shader_stage_index; i++) {
        vkDestroyShaderModule(init->dev, shader_modules[i], NULL);
    }
    return false;
}

void vk_pipeline_destroy(struct vk_init *init,
                         struct vk_pipeline_manager *manager,
                         vk_pipeline_id id) {
    if (id < 0 || id >= MAX_PIPELINES) {
        LOGM(WARN, "pipeline id is invalid: %d", id);
        return;
    }

    struct vk_pipeline *p = &manager->entries[id];

    vkDestroyPipeline(init->dev, p->handle, NULL);
    vkDestroyPipelineLayout(init->dev, p->layout, NULL);
}

static u64 get_file_mtime(const char *path) {
    struct stat s;
    if (stat(path, &s) != 0) {
        LOGM(WARN, "stat failed: %s", path);
        return 0;
    }

    return (u64)s.st_mtime;
}

void vk_pipeline_manager_check_reload(struct vk_init *init,
                                      struct vk_pipeline_manager *manager) {
    for (u32 i = 0; i < manager->count; i++) {
        struct vk_pipeline *p = &manager->entries[i];

        bool dirty = false;
        for (u32 j = 0; j < p->shader_count; j++) {
            struct vk_shader *s = &p->shaders[j];

            u64 mtime = get_file_mtime(s->path);
            if (mtime != s->last_modified) {
                dirty = true;
                s->last_modified = mtime;
            }
        }

        if (dirty) {
            // recompile etc..
        }
    }
}

struct vk_pipeline_desc vk_pipeline_desc_default(void) {
    return (struct vk_pipeline_desc){
        .polygon_mode = VK_POLYGON_MODE_FILL,
        .cull_mode = VK_CULL_MODE_BACK_BIT,
        .front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depth_test = VK_TRUE,
        .depth_write = VK_TRUE,
        .depth_compare_op = VK_COMPARE_OP_LESS,
    };
}

bool vk_pipeline_manager_create(struct vk_init *init,
                                struct vk_pipeline_manager *manager) {
    memset(manager, 0, sizeof(struct vk_pipeline_manager));

    // piepline cache

    return true;
}

void vk_pipeline_manager_destroy(struct vk_init *init,
                                 struct vk_pipeline_manager *manager) {
    for (u32 i = 0; i < manager->count; i++) {
        if (!manager->entries[i].valid) {
            continue;
        }

        vk_pipeline_destroy(init, manager, i);
    }
}

struct vk_pipeline *vk_pipeline_manager_get(struct vk_pipeline_manager *manager,
                                            vk_pipeline_id id) {
    if (id < 0 || id >= MAX_PIPELINES) {
        LOGM(WARN, "pipeline id is invalid: %d", id);
        return NULL;
    }

    return &manager->entries[id];
}
