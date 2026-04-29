
#include <vk/init.h>
#include <vk/pipeline.h>
#include <vk/swapchain.h>

#include <log.h>

#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

static shaderc_include_result *make_include_error(const char *msg) {
    shaderc_include_result *res = malloc(sizeof(*res));
    res->source_name = "";
    res->content = msg;
    res->source_name_length = 0;
    res->content_length = strlen(msg);
    return res;
}

static shaderc_include_result *
include_resolve(void *user_data, const char *requested_source, i32 type,
                const char *requesting_source, size_t include_depth) {
    (void)user_data;

    if (include_depth > 4) {
        LOGM(ERROR, "shader include depth > 4 suspicion of infinite recursion "
                    "stopping parsing");
        return make_include_error("too much recursion in includes");
    }

    if (type != 0) {
        LOGM(ERROR, "no support for include paths in shaders");
        return make_include_error("no support for include paths in shaders");
    }

    char path_copy[PATH_MAX];
    snprintf(path_copy, sizeof(path_copy), "%s", requesting_source);

    char *dir = dirname(path_copy);

    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, requested_source);

    char resolved[PATH_MAX];
    if (!realpath(full_path, resolved)) {
        LOGM(ERROR, "resolved path error: %s", full_path);
        return make_include_error("resolved path error");
    }

    FILE *f = fopen(resolved, "rb");
    if (!f) {
        LOGM(ERROR, "didnt find included file: %s", resolved);
        return make_include_error("path error cant find file");
    }

    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    rewind(f);

    char *buffer = malloc(len + 1);
    fread(buffer, 1, len, f);
    buffer[len] = '\0';

    fclose(f);

    char *resolved_copy = strdup(resolved);

    shaderc_include_result *res = malloc(sizeof(*res));
    res->source_name = resolved_copy;
    res->source_name_length = strlen(resolved_copy);
    res->content = buffer;
    res->content_length = len;

    return res;
}

static void include_release(void *user_data,
                            shaderc_include_result *include_result) {
    (void)user_data;

    free((void *)include_result->content);
    free((void *)include_result->source_name);
    free(include_result);
}

static VkShaderModule shader_compile(struct vk_init *init,
                                     struct vk_pipeline_manager *manager,
                                     const char *path, i32 type) {
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

    shaderc_shader_kind kind = shaderc_glsl_infer_from_source;
    switch (type) {
    case SHADER_TYPE_VERTEX:
        kind = shaderc_glsl_vertex_shader;
        break;
    case SHADER_TYPE_FRAGMENT:
        kind = shaderc_glsl_fragment_shader;
        break;
    default:
        LOGM(WARN, "not valid type trying to detect from source: %s", path);
    }

    shaderc_compilation_result_t result =
        shaderc_compile_into_spv(manager->compiler->handle, buffer, len, kind,
                                 path, "main", manager->compiler->opts);
    if (shaderc_result_get_compilation_status(result) !=
        shaderc_compilation_status_success) {
        const char *msg = shaderc_result_get_error_message(result);
        LOGM(WARN, "shader compilation failed: %s\n%s", path, msg);
        shaderc_result_release(result);
        free(buffer);
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
        free(buffer);
        return VK_NULL_HANDLE;
    }

    shaderc_result_release(result);
    free(buffer);
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

static bool add_shader_stage(VkPipelineShaderStageCreateInfo *shader_stages,
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

    (*index)++;

    return true;
}

static struct vk_shader shader_from_path(const char *path, i32 type) {
    struct vk_shader res = {0};
    if (!path) {
        return res;
    }

    res.type = type;
    res.last_modified = get_file_mtime(path);

    memset(res.path, 0, MAX_SHADER_PATH);
    strncpy(res.path, path, MAX_SHADER_PATH);

    return res;
}

// api call true when called from the outside and not internally
static struct vk_pipeline build_pipeline(struct vk_init *init,
                                         struct vk_swapchain *swapchain,
                                         struct vk_pipeline_manager *manager,
                                         struct vk_pipeline_desc *desc) {
    struct vk_pipeline res = {0};

    res.shaders = malloc(sizeof(struct vk_shader) * MAX_SHADER_STAGES);
    if (!res.shaders) {
        LOGM(ERROR, "failed to allocate pipeline shaders");
        return res;
    }

    res.valid = true;
    res.desc = *desc;

    // max of 2 stages (dont need compute at the moment)
    VkPipelineShaderStageCreateInfo shader_stages[MAX_SHADER_STAGES];

    // for handling the destroying of the modules
    VkShaderModule shader_modules[MAX_SHADER_STAGES];
    u32 shader_stage_index = 0;

    if (desc->has_vertex_shader) {
        VkShaderModule module =
            shader_compile(init, manager, desc->vert_path, SHADER_TYPE_VERTEX);
        if (module == VK_NULL_HANDLE) {
            goto error_path;
        }

        res.shaders[shader_stage_index] =
            shader_from_path(desc->vert_path, SHADER_TYPE_VERTEX);
        add_shader_stage(shader_stages, shader_modules, &shader_stage_index,
                         module, SHADER_TYPE_VERTEX);
    }

    if (desc->has_fragment_shader) {
        VkShaderModule module = shader_compile(init, manager, desc->frag_path,
                                               SHADER_TYPE_FRAGMENT);
        if (module == VK_NULL_HANDLE) {
            goto error_path;
        }

        res.shaders[shader_stage_index] =
            shader_from_path(desc->frag_path, SHADER_TYPE_FRAGMENT);
        add_shader_stage(shader_stages, shader_modules, &shader_stage_index,
                         module, SHADER_TYPE_FRAGMENT);
    }

    res.shader_count = shader_stage_index;

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

    return res;

error_path:
    for (u32 i = 0; i < shader_stage_index; i++) {
        vkDestroyShaderModule(init->dev, shader_modules[i], NULL);
    }

    if (res.shaders) {
        free(res.shaders);
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
    manager->entries[manager->count++] =
        build_pipeline(init, swapchain, manager, desc);

    if (!manager->entries[id].valid) {
        LOGM(ERROR, "pipeline creation failed");
        return NO_PIPELINE;
    }

    return id;
}

static void destroy_pipeline(struct vk_init *init, struct vk_pipeline *p) {
    if (p->shaders) {
        free(p->shaders);
    }

    vkDestroyPipeline(init->dev, p->handle, NULL);
    vkDestroyPipelineLayout(init->dev, p->layout, NULL);
}

void vk_pipeline_destroy(struct vk_init *init,
                         struct vk_pipeline_manager *manager,
                         vk_pipeline_id id) {
    if (id < 0 || id >= MAX_PIPELINES) {
        LOGM(WARN, "pipeline id is invalid: %d", id);
        return;
    }

    struct vk_pipeline *p = &manager->entries[id];
    destroy_pipeline(init, p);
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
            struct vk_pipeline new_pipeline =
                build_pipeline(init, swapchain, manager, &p->desc);
            if (!new_pipeline.valid) {
                LOGM(WARN, "failed to recreate pipeline");
                return;
            }

            vkDeviceWaitIdle(init->dev);
            destroy_pipeline(init, p);
            memcpy(p, &new_pipeline, sizeof(struct vk_pipeline));
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
    };
}

bool vk_pipeline_manager_create(struct vk_pipeline_manager *manager) {
    memset(manager, 0, sizeof(struct vk_pipeline_manager));

    // shader compiler
    manager->compiler = malloc(sizeof(struct vk_shader_compiler));
    manager->compiler->handle = shaderc_compiler_initialize();
    manager->compiler->opts = shaderc_compile_options_initialize();

    // TODO: change to release flags on release builds
    shaderc_compile_options_set_optimization_level(
        manager->compiler->opts, shaderc_optimization_level_zero);
    shaderc_compile_options_set_generate_debug_info(manager->compiler->opts);

    shaderc_compile_options_set_include_callbacks(
        manager->compiler->opts, include_resolve, include_release, NULL);

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

void vk_pipeline_set_shaders(struct vk_pipeline_desc *desc, const char *vert,
                             const char *frag) {
    if (vert) {
        size_t len = strlen(vert);
        if (len >= MAX_SHADER_PATH) {
            LOGM(ERROR, "vertex shader path too long: %s", vert);
            return;
        }

        strncpy(desc->vert_path, vert, MAX_SHADER_PATH);
        desc->has_vertex_shader = true;
    }

    if (frag) {
        size_t len = strlen(frag);
        if (len >= MAX_SHADER_PATH) {
            LOGM(ERROR, "fragment shader path too long: %s", frag);
            return;
        }

        strncpy(desc->frag_path, frag, MAX_SHADER_PATH);
        desc->has_fragment_shader = true;
    }
}

void vk_pipeline_set_vertex_input(struct vk_pipeline_desc *desc,
                                  VkVertexInputBindingDescription *bindings,
                                  u32 bindings_count,
                                  VkVertexInputAttributeDescription *attributes,
                                  u32 attribute_count) {
    if (bindings_count > MAX_BINDING_COUNT) {
        LOGM(ERROR, "bindings_count > MAX_BINDING_COUNT");
        return;
    }

    if (attribute_count > MAX_ATTRIBUTE_COUNT) {
        LOGM(ERROR, "attribute_count > MAX_ATTRIBUTE_COUNT");
        return;
    }

    memset(desc->bindings, 0, sizeof(desc->bindings));
    memcpy(desc->bindings, bindings,
           sizeof(VkVertexInputBindingDescription) * bindings_count);

    memset(desc->attributes, 0, sizeof(desc->attributes));
    memcpy(desc->attributes, attributes,
           sizeof(VkVertexInputAttributeDescription) * attribute_count);

    desc->binding_count = bindings_count;
    desc->attribute_count = attribute_count;
}

void vk_pipeline_set_cull_mode(struct vk_pipeline_desc *desc,
                               VkCullModeFlags cull_mode,
                               VkFrontFace front_face) {
    desc->cull_mode = cull_mode;
    desc->front_face = front_face;
}

void vk_pipeline_set_depth_state(struct vk_pipeline_desc *desc,
                                 VkBool32 depth_write, VkBool32 depth_test,
                                 VkCompareOp depth_compare_op) {
    desc->depth_write = depth_write;
    desc->depth_test = depth_test;
    desc->depth_compare_op = depth_compare_op;
}

void vk_pipeline_set_polygon_mode(struct vk_pipeline_desc *desc,
                                  VkPolygonMode mode) {
    desc->polygon_mode = mode;
}

void vk_pipeline_set_blend_state(
    struct vk_pipeline_desc *desc,
    VkPipelineColorBlendAttachmentState *blend_attachments, u32 blend_count) {

    if (blend_attachments == NULL || blend_count == 0) {
        memset(desc->blend_attachment, 0, sizeof(desc->blend_attachment));
        desc->blend_attachment_count = 0;
        return;
    }

    if (blend_count > MAX_BLEND_COUNT) {
        LOGM(ERROR, "blend_count > MAX_BLEND_COUNT");
        return;
    }

    memset(desc->blend_attachment, 0, sizeof(desc->blend_attachment));
    memcpy(desc->blend_attachment, blend_attachments,
           sizeof(VkPipelineColorBlendAttachmentState) * blend_count);
    desc->blend_attachment_count = blend_count;
}

void vk_pipeline_set_color_attachment(struct vk_pipeline_desc *desc,
                                      u32 count) {
    desc->color_attachment_count = count;
}

void vk_pipeline_set_push_constant(struct vk_pipeline_desc *desc,
                                   u32 push_constant_size,
                                   VkShaderStageFlags push_constant_stages) {
    desc->push_constant_size = push_constant_size;
    desc->push_constant_stages = push_constant_stages;
}

void vk_pipeline_set_descriptor(struct vk_pipeline_desc *desc,
                                VkDescriptorSetLayout *layouts,
                                u32 layout_count) {
    if (!layouts && layout_count) {
        LOGM(ERROR, "layouts == NULL but layout_count != 0");
        return;
    }

    if (layout_count > MAX_DESCRIPTOR_LAYOUT_COUNT) {
        LOGM(ERROR, "layout_count > MAX_DESCRIPTOR_LAYOUT_COUNT");
        return;
    }

    memset(desc->descriptor_set_layouts, 0,
           sizeof(desc->descriptor_set_layouts));
    memcpy(desc->descriptor_set_layouts, layouts,
           sizeof(VkDescriptorSetLayout) * layout_count);
    desc->descriptor_set_layout_count = layout_count;
}
