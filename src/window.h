
#ifndef WINDOW_H
#define WINDOW_H

#include <types.h>

// TODO: idk maybe make it better
struct GLFWwindow;
struct vk_init;
struct window;

typedef void (*window_resize_callback_pfn)(struct window *window, u32 w, u32 h);

struct window {
    u32 width;
    u32 height;

    window_resize_callback_pfn resize;
    void *user_ptr;

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

void window_set_resize_callback(struct window *window,
                                window_resize_callback_pfn func);

void window_set_user_ptr(struct window *window, void *ptr);

void *window_get_user_ptr(struct window *window);

void window_poll_events();

#endif // WINDOW_H
