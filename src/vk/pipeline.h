
#ifndef VULKAN_PIPELINE_H
#define VULKAN_PIPELINE_H

#include <types.h>

#include <vulkan/vulkan.h>

struct vk_init;
struct vk_swapchain;

struct vk_pipeline {
    VkPipeline handle;
    VkPipelineLayout layout;
};

// TODO: change back to static only needed because of shadow pipeline in light.c
// right now
bool vk_shader_module_create(struct vk_init *init, VkShaderModule *module,
                             const char *filename);

bool vk_pipeline_create(struct vk_init *init, struct vk_swapchain *swapchain,
                        struct vk_pipeline *pipeline, const char *vertex_path,
                        const char *fragment_path,
                        VkVertexInputBindingDescription binding_decs,
                        VkVertexInputAttributeDescription *attribute_decs,
                        u32 n_attributes,
                        VkPipelineLayoutCreateInfo layout_info, bool skybox);

void vk_pipeline_destroy(struct vk_init *init, struct vk_pipeline *pipeline);

#endif // VULKAN_PIPELINE_H
