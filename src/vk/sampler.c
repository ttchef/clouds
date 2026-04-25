
#include <vk/init.h>
#include <vk/sampler.h>

#include <log.h>

static bool create_texture_sampler(struct vk_init *init,
                                   struct vk_sampler *sampler) {
    VkSamplerCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = create_info.addressModeU,
        .addressModeW = create_info.addressModeU,
        .mipLodBias = 0.0f,
        .maxAnisotropy = 1.0f,
        .minLod = 0.0f,
        .maxLod = 1.0f,
    };

    if (vkCreateSampler(init->dev, &create_info, NULL, &sampler->handle) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to create sampler");
        return false;
    }

    return true;
}

bool vk_samplers_create(struct vk_init *init, struct vk_samplers *samplers) {
    if (!create_texture_sampler(init, &samplers->texture_sampler)) {
        return false;
    }

    return true;
}

void vk_samplers_destroy(struct vk_init *init, struct vk_samplers *samplers) {
    vkDestroySampler(init->dev, samplers->texture_sampler.handle, NULL);
}
