
#include "window.h"
#include <draw.h>
#include <log.h>
#include <renderer.h>

#include <string.h>

static void push_draw_cmd(struct renderer *r, struct draw_cmd *cmd) {
    struct render_queue *q = &r->render_queue;

    if (q->count + 1 > q->capacity) {
        u32 new_cap = q->capacity * 2;
        struct draw_cmd *new_data =
            realloc(q->cmds, sizeof(struct draw_cmd) * new_cap);
        if (!new_data) {
            LOGM(WARN, "failed to reallocate draw cmds");
            return;
        }

        q->capacity = new_cap;
        q->cmds = new_data;
    }

    memcpy(&q->cmds[q->count++], cmd, sizeof(struct draw_cmd));
}

void draw_box(struct renderer *r, vec3 pos, vec3 scale, vec4 color,
              texture_id texture) {
    struct draw_cmd cmd;
    if (texture == NO_TEXTURE) {
        cmd = (struct draw_cmd){
            .type = DRAW_CMD_TYPE_MODEL_COLOR,
            .pos = pos,
            .scale = scale,
            .model_color.id = r->box_id,
            .model_color.color = color,
        };
    } else {
        cmd = (struct draw_cmd){
            .type = DRAW_CMD_TYPE_MODEL_TEXTURE,
            .pos = pos,
            .scale = scale,
            .model_texture.id = r->box_id,
            .model_texture.texture = texture,
        };
    }

    push_draw_cmd(r, &cmd);
}

void draw_model_color(struct renderer *r, vec3 pos, vec3 scale, vec4 color,
                      model_id model) {
    struct draw_cmd cmd = (struct draw_cmd){
        .type = DRAW_CMD_TYPE_MODEL_COLOR,
        .pos = pos,
        .scale = scale,
        .model_color.color = color,
        .model_color.id = model,
    };

    push_draw_cmd(r, &cmd);
}

void draw_model_texture(struct renderer *r, vec3 pos, vec3 scale,
                        model_id model) {
    struct draw_cmd cmd = (struct draw_cmd){
        .type = DRAW_CMD_TYPE_MODEL_TEXTURE,
        .pos = pos,
        .scale = scale,
        .model_texture.id = model,
        .model_texture.texture = NO_TEXTURE, // model has texture
    };

    push_draw_cmd(r, &cmd);
}

void draw_cloud(struct renderer *r, vec3 pos, vec3 scale, vec4 color) {
    struct draw_cmd cmd = (struct draw_cmd){
        .type = DRAW_CMD_TYPE_CLOUD,
        .pos = pos,
        .scale = scale,
        .cloud.color = color,
    };

    push_draw_cmd(r, &cmd);
}

void draw_cmds(struct renderer *r, struct vk_frame_data *data, bool shadow_pass,
               struct shadow_pc *shadow_pc) {
    struct render_queue *q = &r->render_queue;

    for (u32 i = 0; i < q->count; i++) {
        struct draw_cmd *cmd = &q->cmds[i];

        matrix translate_m =
            math_matrix_translate(cmd->pos.x, cmd->pos.y, cmd->pos.z);
        matrix scale_m =
            math_matrix_scale(cmd->scale.x, cmd->scale.y, cmd->scale.z);
        matrix model = math_matrix_mul(translate_m, scale_m);

        switch (cmd->type) {
        case DRAW_CMD_TYPE_MODEL_COLOR: {
            if (shadow_pass) {
                vkCmdBindPipeline(data->cmd_buffer,
                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  r->light_manager.shadow_pip.handle);

                shadow_pc->model = model;
                vkCmdPushConstants(data->cmd_buffer,
                                   r->light_manager.shadow_pip.layout,
                                   VK_SHADER_STAGE_VERTEX_BIT, 0,
                                   sizeof(struct shadow_pc), shadow_pc);

                vkCmdBindDescriptorSets(
                    data->cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    r->light_manager.shadow_pip.layout, 0, 1,
                    &r->descriptors.sets[r->cmd.frame_idx], 0, NULL);
            } else {
                vkCmdBindPipeline(data->cmd_buffer,
                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  r->model_color_pip.handle);

                struct model_color_pc push_constant = {
                    .model = model,
                    .cam_pos = (vec4){r->camera.pos.x, r->camera.pos.y,
                                      r->camera.pos.z, 0.0},
                    .color = cmd->model_color.color,
                };

                vkCmdPushConstants(
                    data->cmd_buffer, r->model_color_pip.layout,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    0, sizeof(struct model_color_pc), &push_constant);

                vkCmdBindDescriptorSets(
                    data->cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    r->model_color_pip.layout, 0, 1,
                    &r->descriptors.sets[r->cmd.frame_idx], 0, NULL);
            }

            VkDeviceSize offsets[] = {0};

            vkCmdBindVertexBuffers(
                data->cmd_buffer, 0, 1,
                &r->models[cmd->model_color.id].vertex_buffer.handle, offsets);
            vkCmdBindIndexBuffer(
                data->cmd_buffer,
                r->models[cmd->model_color.id].index_buffer.handle, 0,
                VK_INDEX_TYPE_UINT16);

            vkCmdDrawIndexed(data->cmd_buffer,
                             r->models[cmd->model_color.id].n_index, 1, 0, 0,
                             0);
        } break;
        case DRAW_CMD_TYPE_MODEL_TEXTURE: {
            // TODO: only for now just exit
            if (r->models[cmd->model_texture.id].texture == NO_TEXTURE &&
                cmd->model_texture.texture == NO_TEXTURE) {
                LOGM(ERROR, "render model has no texture");
                exit(1);
            }

            texture_id texture = NO_TEXTURE;
            if (r->models[cmd->model_texture.id].texture != NO_TEXTURE) {
                texture = r->models[cmd->model_texture.id].texture;
            }
            if (cmd->model_texture.texture != NO_TEXTURE) {
                if (texture != NO_TEXTURE) {
                    LOGM(WARN,
                         "model_texture: %d has two textures one in the "
                         "model"
                         "and one extern",
                         cmd->model_texture.id);
                }
                texture = cmd->model_texture.texture;
            }

            if (shadow_pass) {
                vkCmdBindPipeline(data->cmd_buffer,
                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  r->light_manager.shadow_pip.handle);

                shadow_pc->model = model;
                vkCmdPushConstants(data->cmd_buffer,
                                   r->light_manager.shadow_pip.layout,
                                   VK_SHADER_STAGE_VERTEX_BIT, 0,
                                   sizeof(struct shadow_pc), shadow_pc);

                vkCmdBindDescriptorSets(
                    data->cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    r->light_manager.shadow_pip.layout, 0, 1,
                    &r->descriptors.sets[r->cmd.frame_idx], 0, NULL);
            } else {
                vkCmdBindPipeline(data->cmd_buffer,
                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  r->model_texture_pip.handle);

                struct model_texture_pc push_constant = {
                    .model = model,
                    .cam_pos = (vec4){r->camera.pos.x, r->camera.pos.y,
                                      r->camera.pos.z, 0.0},
                    .texture_index = texture,
                };

                vkCmdPushConstants(
                    data->cmd_buffer, r->model_texture_pip.layout,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    0, sizeof(struct model_texture_pc), &push_constant);

                vkCmdBindDescriptorSets(
                    data->cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    r->model_texture_pip.layout, 0, 1,
                    &r->descriptors.sets[r->cmd.frame_idx], 0, NULL);
            }

            VkDeviceSize offsets[] = {0};

            vkCmdBindVertexBuffers(
                data->cmd_buffer, 0, 1,
                &r->models[cmd->model_texture.id].vertex_buffer.handle,
                offsets);
            vkCmdBindIndexBuffer(
                data->cmd_buffer,
                r->models[cmd->model_texture.id].index_buffer.handle, 0,
                VK_INDEX_TYPE_UINT16);

            vkCmdDrawIndexed(data->cmd_buffer,
                             r->models[cmd->model_texture.id].n_index, 1, 0, 0,
                             0);
        }; break;
        case DRAW_CMD_TYPE_CLOUD: {
            if (shadow_pass) {
                return;
            }

            vkCmdBindPipeline(data->cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              r->cloud_pip.handle);

            vkCmdBindDescriptorSets(
                data->cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                r->cloud_pip.layout, 0, 1,
                &r->descriptors.sets[r->cmd.frame_idx], 0, NULL);

            struct cloud_pc push_constant = {
                .model = model,
                .cam_pos = (vec4){r->camera.pos.x, r->camera.pos.y,
                                  r->camera.pos.z, 0.0},
                .color = cmd->cloud.color,
                .time = window_get_time(),
            };

            vkCmdPushConstants(data->cmd_buffer, r->cloud_pip.layout,
                               VK_SHADER_STAGE_VERTEX_BIT |
                                   VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(struct cloud_pc), &push_constant);

            VkDeviceSize offsets[] = {0};

            vkCmdBindVertexBuffers(data->cmd_buffer, 0, 1,
                                   &r->models[r->box_id].vertex_buffer.handle,
                                   offsets);
            vkCmdBindIndexBuffer(data->cmd_buffer,
                                 r->models[r->box_id].index_buffer.handle, 0,
                                 VK_INDEX_TYPE_UINT16);

            vkCmdDrawIndexed(data->cmd_buffer, r->models[r->box_id].n_index, 1,
                             0, 0, 0);
        } break;
        }
    }

    if (!shadow_pass) {
        q->count = 0;
    }
}
