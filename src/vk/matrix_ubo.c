
#include <vk/matrix_ubo.h>

#include <renderer.h>
#include <shader_shared.h>

#include <string.h>

bool vk_matrix_ubo_create(struct renderer *r, struct vk_matrix_ubo *m) {
    VkDescriptorBufferInfo buffer_info = {
        .offset = 0,
        .range = sizeof(struct vk_matrix_ubo_data),
    };

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &buffer_info,
        .descriptorCount = 1,
        .dstBinding = GLOBAL_DESC_MATRIX_BINDING,
    };

    for (i32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
        m->buffers[i] = vk_buffer_create_host_visible(
            &r->init, sizeof(struct vk_matrix_ubo_data),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

        buffer_info.buffer = m->buffers[i].handle;
        write.dstSet = r->descriptors.sets[i];

        vkUpdateDescriptorSets(r->init.dev, 1, &write, 0, NULL);
    }

    return true;
}

void vk_matrix_ubo_destroy(struct renderer *r, struct vk_matrix_ubo *m) {
    for (i32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vmaDestroyBuffer(r->init.allocator, m->buffers[i].handle,
                         m->buffers[i].alloc);
    }
}

void vk_matrix_ubo_sync_data(struct renderer *r, struct vk_matrix_ubo *m) {
    memcpy(m->buffers[r->cmd.frame_idx].host_visible.mapped, &m->data,
           sizeof(struct vk_matrix_ubo_data));
}
