
#include <log.h>
#include <renderer.h>
#include <texture.h>

#include <stbi/stb_image.h>

#include <string.h>

bool texture_manager_create(struct renderer *r,
                            struct texture_manager *manager) {
    (void)r;
    memset(manager, 0, sizeof(struct texture_manager));

    return true;
}

void texture_manager_destroy(struct renderer *r,
                             struct texture_manager *manager) {
    for (u32 i = 0; i < MAX_TEXTURES; i++) {
        if (manager->textures[i].valid) {
            texture_destroy(r, i);
        }
    }
}

texture_id texture_create(struct renderer *r, u32 width, u32 height,
                          void *data) {
    struct vk_init *init = &r->init;
    struct texture_manager *manager = &r->texture_manager;

    struct texture res = {
        .valid = true,
    };

    vk_image_create(init, &res.image, width, height, 1, VK_FORMAT_R8G8B8A8_SRGB,
                    VK_IMAGE_USAGE_SAMPLED_BIT |
                        VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                    IMAGE_TYPE_2D);
    vk_image_upload_data(init, &res.image, width * height * 4, data, width,
                         height, 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_ACCESS_SHADER_READ_BIT, NULL);

    // TODO: better datastructure ???
    i32 i = 0;
    for (; i < MAX_TEXTURES; i++) {
        if (!manager->textures[i].valid) {
            break;
        }
    }

    texture_id id = i;
    manager->textures[i] = res;

    VkDescriptorImageInfo image_info = {
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .imageView = res.image.view,
        .sampler = r->samplers.texture_sampler.handle,
    };

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = GLOBAL_DESC_TEXTURE_BINDING,
        .dstArrayElement = id,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &image_info,
    };

    for (i32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
        write.dstSet = r->descriptors.sets[i];

        vkUpdateDescriptorSets(init->dev, 1, &write, 0, NULL);
    }

    return id;
}

texture_id texture_create_file(struct renderer *r, const char *path) {
    i32 width, height, bpp;
    u8 *data = stbi_load(path, &width, &height, &bpp, 4);
    if (!data) {
        LOGM(ERROR, "failed to load texture: %s", path);
        return NO_TEXTURE;
    }

    texture_id id = texture_create(r, width, height, data);

    stbi_image_free(data);

    return id;
}

void texture_destroy(struct renderer *r, texture_id id) {
    if (id > MAX_TEXTURES || id < 0) {
        LOGM(ERROR, "texture id is not valid");
        return;
    }

    struct texture *t = &r->texture_manager.textures[id];

    vk_image_destroy(&r->init, &t->image);
    t->valid = false;
}
