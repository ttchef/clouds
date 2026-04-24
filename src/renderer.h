
#ifndef RENDERER_H
#define RENDERER_H

#include <vk/init.h>
#include <window.h>

struct renderer {
    struct vk_init init;
};

bool renderer_init(struct renderer *r, struct window *window);
void renderer_deint(struct renderer *r);

#endif // RENDERER_H
