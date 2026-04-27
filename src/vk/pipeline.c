
#include <shaderc/status.h>
#include <vk/init.h>
#include <vk/pipeline.h>
#include <vk/swapchain.h>

#include <log.h>

#include <string.h>
#include <sys/stat.h>

#include <shaderc/shaderc.h>
#include <vulkan/vulkan_core.h>

// total amount of maximun shader stages
// 2 right now because there is only a maximum
// of a vertex and a fragment shader
// no compute tesselation or geometry
#define MAX_SHADER_STAGES 2

struct vk_shader_compiler {
    shaderc_compiler_t handle;
    shaderc_compile_options_t opts;
};

VkShaderModule vk_shader_module_create(struct vk_init *init, const char *path) {
    FILE *shader_fd = fopen(path, "rb");
    if (!shader_fd) {
        LOGM(ERROR, "failed to open shader file: %s", path);
        return VK_NULL_HANDLE;
    }

    fseek(shader_fd, 0, SEEK_END);
    i64 shader_size = ftell(shader_fd);
    rewind(shader_fd);

    if ((shader_size & 0x03) != 0) {
        LOGM(ERROR, "shader compile is not a multiple of 4");
        fclose(shader_fd);
        return VK_NULL_HANDLE;
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

    VkShaderModule module;
    if (vkCreateShaderModule(init->dev, &create_info, NULL, &module) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to create shader module: %s", path);
        return VK_NULL_HANDLE;
    }

    return module;
}

static VkShaderModule vk_shader_compile(struct vk_init *init,
                                        struct vk_pipeline_manager *manager,
                                        const char *path,
                                        shaderc_shader_kind kind) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        LOGM(WARN, "couldnt open: %s", path);
        return VK_NULL_HANDLE;
    }

    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    rewind(f);

    char *buffer = malloc(len + 1);

    fread(buffer, 1, len, f);
    buffer[len] = '\0';

    fclose(f);

    shaderc_compilation_result_t result =
        shaderc_compile_into_spv(manager->compiler->handle, buffer, len, kind,
                                 path, "main", manager->compiler->opts);
    if (shaderc_result_get_compilation_status(result) !=
        shaderc_compilation_status_success) {
        shaderc_result_release(result);
        LOGM(WARN, "shader compilation failed: %s", path);
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shaderc_result_get_length(result),
        .pCode = (const u32 *)shaderc_result_get_bytes(result),
    };

    VkShaderModule module;
    if (vkCreateShaderModule(init->dev, &create_info, NULL, &module) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to create shader module: %s", path);
        shaderc_result_release(result);
        return VK_NULL_HANDLE;
    }

    shaderc_result_release(result);
    return module;
}

static u64 get_file_mtime(const char *path) {
    struct stat s;
    if (stat(path, &s) != 0) {
        LOGM(WARN, "stat failed: %s", path);
        return 0;
    }

    return (u64)s.st_mtime;
}

static bool add_shader_stage(struct vk_pipeline *pip,
                             VkPipelineShaderStageCreateInfo *shader_stages,
                             VkShaderModule *shader_modules, u32 *index,
                             VkShaderModule module, i32 type) {
    if (*index >= MAX_SHADER_STAGES) {
        LOGM(WARN, "inavlid index");
        return false;
    }

    u32 i = *index;

    shader_modules[i] = module;
    shader_stages[i] = (VkPipelineShaderStageCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = (type == SHADER_TYPE_VERTEX) ? VK_SHADER_STAGE_VERTEX_BIT
                                              : VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = shader_modules[i],
        .pName = "main",
    };

    pip->shaders[i] = (struct vk_shader){
        .type = type, .last_modified = get_file_mtime(const char *path)};

    (*index)++;

    return true;
}

// api call true when called from the outside and not internally
static struct vk_pipeline build_pipeline(struct vk_init *init,
                                         struct vk_swapchain *swapchain,
                                         struct vk_pipeline_desc *desc,
                                         bool api_call) {
    struct vk_pipeline res = {0};

    if (api_call) {
        res.shaders = malloc(sizeof(struct vk_shader) * MAX_SHADER_STAGES);
    }

    // max of 2 stages (dont need compute at the moment)
    VkPipelineShaderStageCreateInfo shader_stages[MAX_SHADER_STAGES];

    // for handling the destroying of the modules
    VkShaderModule shader_modules[MAX_SHADER_STAGES];
    u32 shader_stage_index = 0;

    if (desc->vert_path) {
        VkShaderModule module = vk_shader_module_create(init, desc->vert_path);
        if (module == VK_NULL_HANDLE) {
            goto error_path;
        }

        add_shader_stage(&res, shader_stages, shader_modules,
                         &shader_stage_index, module, SHADER_TYPE_VERTEX);
    } else if (desc->vert_module != VK_NULL_HANDLE) {
        add_shader_stage(&res, shader_stages, shader_modules,
                         &shader_stage_index, desc->vert_module,
                         SHADER_TYPE_VERTEX);
    }

    if (desc->frag_path) {
        VkShaderModule module = vk_shader_module_create(init, desc->frag_path);
        if (module == VK_NULL_HANDLE) {
            goto error_path;
        }

        add_shader_stage(&res, shader_stages, shader_modules,
                         &shader_stage_index, module, SHADER_TYPE_FRAGMENT);
    } else if (desc->frag_module != VK_NULL_HANDLE) {
        add_shader_stage(&res, shader_stages, shader_modules,
                         &shader_stage_index, desc->frag_module,
                         SHADER_TYPE_FRAGMENT);
    }

    VkPipelineRenderingCreateInfo dynamic_rendering = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = desc->color_attachment_count,
        .pColorAttachmentFormats =
            desc->color_attachment_count == 0 ? NULL : &swapchain->fmt,
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
        .frontFace = desc->front_face,
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
        .pPushConstantRanges =
            desc->push_constant_size == 0 ? NULL : &push_constant_range,
    };

    if (vkCreatePipelineLayout(init->dev, &pipeline_layout_create_info, NULL,
                               &res.layout) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create pipeline layout");
        goto error_path;
    }

    VkGraphicsPipelineCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &dynamic_rendering,
        .layout = res.layout,
        .stageCount = shader_stage_index,
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
                                  &res.handle) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create graphics pipeline");
        goto error_path;
    }

    for (u32 i = 0; i < shader_stage_index; i++) {
        vkDestroyShaderModule(init->dev, shader_modules[i], NULL);
    }

    res.valid = true;
    res.desc = *desc;

    return res;

error_path:
    for (u32 i = 0; i < shader_stage_index; i++) {
        vkDestroyShaderModule(init->dev, shader_modules[i], NULL);
    }
    return (struct vk_pipeline){0};
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

    vk_pipeline_id id = manager->count;
    manager->entries[manager->count++] = build_pipeline(init, swapchain, desc);
    if (!manager->entries[id].valid) {
        LOGM(ERROR, "pipeline creation failed");
        return NO_PIPELINE;
    }

    return id;
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

static shaderc_shader_kind kind_from_path(const char *path) {
    const char *ext = strrchr(path, '-');
    if (!ext) {
        LOGM(INFO,
             "cloudnt detect shader type of: %s inferring from source code",
             path);
        return shaderc_glsl_infer_from_source;
    }

    if (strcmp(ext, "-vert.spv") == 0)
        return shaderc_vertex_shader;
    if (strcmp(ext, "-frag.spv") == 0)
        return shaderc_fragment_shader;
    if (strcmp(ext, "-comp.vert") == 0)
        return shaderc_compute_shader;

    LOGM(INFO, "cloudnt detect shader type of: %s inferring from source code",
         path);
    return shaderc_glsl_infer_from_source;
}

void vk_pipeline_manager_check_reload(struct vk_init *init,
                                      struct vk_swapchain *swapchain,
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
            VkShaderModule modules[p->shader_count];
            for (u32 j = 0; j < p->shader_count; j++) {
                struct vk_shader *s = &p->shaders[j];
                modules[j] = vk_shader_compile(init, manager, s->path,
                                               kind_from_path(s->path));
                if (modules[j] == VK_NULL_HANDLE) {
                    LOGM(WARN, "failed to hot reload shader: %s", s->path);
                    continue;
                }

                switch (s->type) {
                case SHADER_TYPE_VERTEX: {
                    p->desc.vert_path = NULL;
                    p->desc.vert_module = modules[j];
                } break;
                case SHADER_TYPE_FRAGMENT: {
                    p->desc.frag_path = NULL;
                    p->desc.frag_module = modules[j];
                } break;
                }
            }

            vk_pipeline_id new_pipeline =
                vk_pipeline_create(init, swapchain, manager, &p->desc);
            if (new_pipeline == NO_PIPELINE) {
                LOGM(WARN, "failed to recreate pipeline");
                return;
            }
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
        .color_attachment_count = 1,
        .vert_module = VK_NULL_HANDLE,
        .frag_module = VK_NULL_HANDLE,
    };
}

bool vk_pipeline_manager_create(struct vk_init *init,
                                struct vk_pipeline_manager *manager) {
    memset(manager, 0, sizeof(struct vk_pipeline_manager));

    // shader compiler
    manager->compiler = malloc(sizeof(struct vk_shader_compiler));
    manager->compiler->handle = shaderc_compiler_initialize();
    manager->compiler->opts = shaderc_compile_options_initialize();

    // TODO: change to release flags on release builds
    shaderc_compile_options_set_optimization_level(
        manager->compiler->opts, shaderc_optimization_level_zero);
    shaderc_compile_options_set_generate_debug_info(manager->compiler->opts);

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

    // destroy compiler
    shaderc_compile_options_release(manager->compiler->opts);
    shaderc_compiler_release(manager->compiler->handle);
    free(manager->compiler);
}

struct vk_pipeline *vk_pipeline_manager_get(struct vk_pipeline_manager *manager,
                                            vk_pipeline_id id) {
    if (id < 0 || id >= MAX_PIPELINES) {
        LOGM(WARN, "pipeline id is invalid: %d", id);
        return NULL;
    }

    return &manager->entries[id];
}
