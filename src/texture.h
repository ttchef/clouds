
#ifndef TEXTURE_H
#define TEXTURE_H

#include <shader_shared.h>
#include <types.h>

#include <vk/image.h>

struct renderer;

#define NO_TEXTURE -1

typedef i32 texture_id;

struct texture {
    struct vk_image image;
    bool valid;
};

struct texture_manager {
    struct texture textures[MAX_TEXTURES];
};

bool texture_manager_create(struct renderer *r,
                            struct texture_manager *manager);

void texture_manager_destroy(struct renderer *r,
                             struct texture_manager *manager);

// bpp always == 4 asumed
texture_id texture_create(struct renderer *r, u32 width, u32 height,
                          void *data);

texture_id texture_create_file(struct renderer *r, const char *path);

void texture_destroy(struct renderer *r, texture_id id);

#endif // TEXTURE_H
