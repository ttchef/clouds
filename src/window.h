
#ifndef WINDOW_H
#define WINDOW_H

#include <types.h>

// TODO: idk maybe make it better
struct GLFWwindow;
struct vk_init;

struct window {
    u32 width;
    u32 height;

    struct GLFWwindow *handle;
};

bool window_init(void);

bool window_create(struct window *window, u32 width, u32 height,
                   const char *title);

void window_destroy(struct window *window);

void window_deinit(void);

bool window_should_close(struct window *window);

void window_create_surface(struct window *window, struct vk_init *init);

const char **window_get_instance_exts(u32 *n_exts);

f32 window_get_time();

#endif // WINDOW_H
