
#ifndef VULKAN_PIPELINE_H
#define VULKAN_PIPELINE_H

#include <types.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define MAX_SHADER_PATH 256
#define MAX_BINDING_COUNT 3
#define MAX_ATTRIBUTE_COUNT 5
#define MAX_BLEND_COUNT 3
#define MAX_DESCRIPTOR_LAYOUT_COUNT 3

#define MAX_PIPELINES 10
#define NO_PIPELINE -1

struct vk_init;
struct vk_swapchain;

typedef i32 vk_pipeline_id;

struct vk_pipeline_desc {
    // glsl shader files
    // not spv
    char vert_path[MAX_SHADER_PATH + 1];
    char frag_path[MAX_SHADER_PATH + 1];

    bool has_vertex_shader;
    bool has_fragment_shader;

    // vertex input
    VkVertexInputBindingDescription bindings[MAX_BINDING_COUNT];
    u32 binding_count;
    VkVertexInputAttributeDescription attributes[MAX_ATTRIBUTE_COUNT];
    u32 attribute_count;

    // rasterization
    VkPolygonMode polygon_mode;
    VkCullModeFlags cull_mode;
    VkFrontFace front_face;

    // depth
    VkBool32 depth_write;
    VkBool32 depth_test;
    VkCompareOp depth_compare_op;

    // blending
    VkPipelineColorBlendAttachmentState blend_attachment[MAX_BLEND_COUNT];
    u32 blend_attachment_count;
    u32 color_attachment_count;

    // push constant
    u32 push_constant_size;
    VkShaderStageFlags push_constant_stages;

    // descriptor sets
    u32 descriptor_set_layout_count;
    VkDescriptorSetLayout descriptor_set_layouts[MAX_DESCRIPTOR_LAYOUT_COUNT];
};

enum {
    SHADER_TYPE_VERTEX,
    SHADER_TYPE_FRAGMENT,
};

struct vk_shader {
    char path[MAX_SHADER_PATH];
    u64 last_modified;
    i32 type;
};

struct vk_pipeline {
    struct vk_pipeline_desc desc;
    struct vk_shader *shaders;
    u32 shader_count;

    VkPipeline handle;
    VkPipelineLayout layout;
    bool valid;
};

// opaque pointer only access in pipeline.c
struct vk_shader_compiler;

struct vk_pipeline_manager {
    struct vk_pipeline entries[MAX_PIPELINES];
    u32 count;

    VkPipelineCache cache;
    struct vk_shader_compiler *compiler;
};

// returns NO_PIPELINE on failure
vk_pipeline_id vk_pipeline_create(struct vk_init *init,
                                  struct vk_swapchain *swapchain,
                                  struct vk_pipeline_manager *manager,
                                  struct vk_pipeline_desc *desc);

void vk_pipeline_destroy(struct vk_init *init,
                         struct vk_pipeline_manager *manager,
                         vk_pipeline_id id);

void vk_pipeline_manager_check_reload(struct vk_init *init,
                                      struct vk_swapchain *swapchain,
                                      struct vk_pipeline_manager *manager);

// manager
bool vk_pipeline_manager_create(struct vk_init *init,
                                struct vk_pipeline_manager *manager);

void vk_pipeline_manager_destroy(struct vk_init *init,
                                 struct vk_pipeline_manager *manager);

struct vk_pipeline *vk_pipeline_manager_get(struct vk_pipeline_manager *manager,
                                            vk_pipeline_id id);
// maanger end

// builder
struct vk_pipeline_desc vk_pipeline_desc_default(void);

void vk_pipeline_set_shaders(struct vk_pipeline_desc *desc, const char *vert,
                             const char *frag);

void vk_pipeline_set_vertex_input(struct vk_pipeline_desc *desc,
                                  VkVertexInputBindingDescription *bindings,
                                  u32 bindings_count,
                                  VkVertexInputAttributeDescription *attributes,
                                  u32 attribute_count);

void vk_pipeline_set_cull_mode(struct vk_pipeline_desc *desc,
                               VkCullModeFlags cull_mode,
                               VkFrontFace front_face);

void vk_pipeline_set_depth_state(struct vk_pipeline_desc *desc,
                                 VkBool32 depth_write, VkBool32 depth_test,
                                 VkCompareOp depth_compare_op);

void vk_pipeline_set_polygon_mode(struct vk_pipeline_desc *desc,
                                  VkPolygonMode mode);

void vk_pipeline_set_blend_state(
    struct vk_pipeline_desc *desc,
    VkPipelineColorBlendAttachmentState *blend_attachments, u32 blend_count);

void vk_pipeline_set_color_attachment(struct vk_pipeline_desc *desc, u32 count);

void vk_pipeline_set_push_constant(struct vk_pipeline_desc *desc,
                                   u32 push_constant_size,
                                   VkShaderStageFlags push_constant_stages);

void vk_pipeline_set_descriptor(struct vk_pipeline_desc *desc,
                                VkDescriptorSetLayout *layouts,
                                u32 layout_count);

#endif // VULKAN_PIPELINE_H
