
#ifndef VULKAN_PIPELINE_H
#define VULKAN_PIPELINE_H

#include <types.h>

#include <vulkan/vulkan.h>

#define SHADER_MAX_PATH_LEN 256
#define MAX_PIPELINES 10
#define NO_PIPELINE -1

struct vk_init;
struct vk_swapchain;

typedef i32 vk_pipeline_id;

struct vk_pipeline_desc {
    const char *vert_path;
    const char *frag_path;

    // vertex input
    VkVertexInputBindingDescription *bindings;
    u32 binding_count;
    VkVertexInputAttributeDescription *attributes;
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
    VkPipelineColorBlendAttachmentState *blend_attachment;
    u32 blend_attachment_count;

    // push constant
    u32 push_constant_size;
    VkShaderStageFlags push_constant_stages;

    // descriptor sets
    u32 descriptor_set_layout_count;
    VkDescriptorSetLayout *descriptor_set_layouts;
};

struct vk_shader {
    char path[SHADER_MAX_PATH_LEN];
    u64 last_modified;
};

struct vk_pipeline {
    struct vk_pipeline_desc desc;
    struct vk_shader *shaders;
    u32 shader_count;

    VkPipeline handle;
    VkPipelineLayout layout;
    bool valid;
};

struct vk_pipeline_manager {
    struct vk_pipeline entries[MAX_PIPELINES];
    u32 count;

    VkPipelineCache cache;
};

// TODO: change back to static only needed because of shadow pipeline in light.c
// right now
bool vk_shader_module_create(struct vk_init *init, VkShaderModule *module,
                             const char *filename);

// returns NO_PIPELINE on failure
vk_pipeline_id vk_pipeline_create(struct vk_init *init,
                                  struct vk_swapchain *swapchain,
                                  struct vk_pipeline_manager *manager,
                                  struct vk_pipeline_desc *desc);

void vk_pipeline_destroy(struct vk_init *init,
                         struct vk_pipeline_manager *manager,
                         vk_pipeline_id id);

void vk_pipeline_manager_check_reload(struct vk_init *init,
                                      struct vk_pipeline_manager *manager);

struct vk_pipeline_desc vk_pipeline_desc_default(void);

bool vk_pipeline_manager_create(struct vk_init *init,
                                struct vk_pipeline_manager *manager);

void vk_pipeline_manager_destroy(struct vk_init *init,
                                 struct vk_pipeline_manager *manager);

#endif // VULKAN_PIPELINE_H
