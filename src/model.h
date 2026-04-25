
#ifndef MODEL_H
#define MODEL_H

#include <texture.h>
#include <types.h>

#include <vk/buffer.h>

struct renderer;

#define NO_MODEL -1

typedef i32 model_id;

struct model {
    struct vk_buffer vertex_buffer;
    struct vk_buffer index_buffer;
    u32 n_index;

    texture_id texture;
    bool valid;
};

// TODO: implement so you can load from memory
// model_id model_create(struct renderer *r, ...)

model_id model_create_file(struct renderer *r, const char *path);

void model_destroy(struct renderer *r, model_id id);

bool model_set_texture(struct renderer *r, model_id model, texture_id texture);

#endif // MODEL_H
