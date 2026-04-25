
#include <vk/descriptor.h>
#include <vk/init.h>

#include <log.h>
#include <shader_shared.h>

bool vk_descriptor_create(struct vk_init *init, struct vk_descriptor *desc) {
    VkDescriptorPoolSize pool_sizes[] = {
        {
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            MAX_TEXTURES * FRAMES_IN_FLIGHT +
                // shadow maps
                (MAX_DIRECTIONAL_LIGHTS + MAX_POINT_LIGHTS + MAX_SPOT_LIGHTS) *
                    FRAMES_IN_FLIGHT +

                // skybox
                FRAMES_IN_FLIGHT +

                // noise
                FRAMES_IN_FLIGHT,
        },
        {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            FRAMES_IN_FLIGHT * 2,
        },
    };

    VkDescriptorPoolCreateInfo pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = FRAMES_IN_FLIGHT,
        .poolSizeCount = ARRAY_COUNT(pool_sizes),
        .pPoolSizes = pool_sizes,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
    };

    if (vkCreateDescriptorPool(init->dev, &pool_create_info, NULL,
                               &desc->pool) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create descriptor pool");
        return false;
    }

    // TODO: scalable way of adding bindings
    VkDescriptorSetLayoutBinding bindings[] = {
        {GLOBAL_DESC_TEXTURE_BINDING, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         MAX_TEXTURES, VK_SHADER_STAGE_FRAGMENT_BIT, 0},

        {GLOBAL_DESC_LIGHT_BINDING, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
         VK_SHADER_STAGE_FRAGMENT_BIT, 0},

        {GLOBAL_DESC_MATRIX_BINDING, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
         VK_SHADER_STAGE_VERTEX_BIT, 0},

        {GLOBAL_DESC_SHADOW_DIRECTIONAL_BINDING,
         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_DIRECTIONAL_LIGHTS,
         VK_SHADER_STAGE_FRAGMENT_BIT, 0},

        {GLOBAL_DESC_SHADOW_POINT_BINDING,
         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_POINT_LIGHTS,
         VK_SHADER_STAGE_FRAGMENT_BIT, 0},

        {GLOBAL_DESC_SHADOW_SPOT_BINDING,
         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_SPOT_LIGHTS,
         VK_SHADER_STAGE_FRAGMENT_BIT, 0},

        {GLOBAL_DESC_SKYBOX_BINDING, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         1, VK_SHADER_STAGE_FRAGMENT_BIT, 0},

        {GLOBAL_DESC_NOISE_3D_BINDING,
         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
         VK_SHADER_STAGE_FRAGMENT_BIT, 0},
    };

    VkDescriptorBindingFlags binding_flags[] = {
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
        0,
        0,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
        0,
        0,
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info = {
        .sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = ARRAY_COUNT(bindings),
        .pBindingFlags = binding_flags,
    };

    VkDescriptorSetLayoutCreateInfo layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pBindings = bindings,
        .bindingCount = ARRAY_COUNT(bindings),
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .pNext = &flags_info,
    };

    if (vkCreateDescriptorSetLayout(init->dev, &layout_create_info, NULL,
                                    &desc->layout) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create descriptor layout");
        return false;
    }

    u32 counts[FRAMES_IN_FLIGHT];

    for (u32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
        counts[i] = MAX_TEXTURES;
    }

    VkDescriptorSetVariableDescriptorCountAllocateInfo count_info = {
        .sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = FRAMES_IN_FLIGHT,
        .pDescriptorCounts = counts,
    };

    // to allocate all in one call
    VkDescriptorSetLayout layouts[] = {
        desc->layout,
        desc->layout,
        desc->layout,
    };

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = &count_info,
        .descriptorPool = desc->pool,
        .descriptorSetCount = FRAMES_IN_FLIGHT,
        .pSetLayouts = layouts,
    };

    if (vkAllocateDescriptorSets(init->dev, &alloc_info, desc->sets) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to allocate descriptor sets");
        return false;
    }

    return true;
}

void vk_descriptor_destroy(struct vk_init *init, struct vk_descriptor *desc) {
    vkDestroyDescriptorSetLayout(init->dev, desc->layout, NULL);
    vkDestroyDescriptorPool(init->dev, desc->pool, NULL);
}
