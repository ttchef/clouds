
#ifndef DRAW_H
#define DRAW_H

#include <cmath.h>
#include <model.h>
#include <texture.h>
#include <types.h>

#include <vk/command.h>

struct renderer;

enum {
    DRAW_CMD_TYPE_MODEL_COLOR,
    DRAW_CMD_TYPE_MODEL_TEXTURE,
    DRAW_CMD_TYPE_CLOUD,
};

struct draw_cmd {
    i32 type;
    vec3 pos;
    vec3 scale;

    union {
        struct {
            model_id id;
            vec4 color;
        } model_color;

        struct {
            model_id id;

            // only for special purposes
            // like the box where the model
            // doesnt have the texture itself
            texture_id texture;
        } model_texture;

        struct {
            vec4 color;
        } cloud;
    };
};

struct render_queue {
    u32 count;
    u32 capacity;
    struct draw_cmd *cmds;
};

void draw_init(struct render_queue *render_queue);

void draw_box(struct renderer *r, vec3 pos, vec3 scale, vec4 color,
              texture_id texture);

// renders the model in the color specified
void draw_model_color(struct renderer *r, vec3 pos, vec3 scale, vec4 color,
                      model_id model);

void draw_model_texture(struct renderer *r, vec3 pos, vec3 scale,
                        model_id model);

void draw_cloud(struct renderer *r, vec3 pos, vec3 scale, vec4 color);

void draw_cmds(struct renderer *r, struct vk_frame_data *data, bool shadow_pass,
               struct shadow_pc *shadow_pc);

#endif // DRAW_H
