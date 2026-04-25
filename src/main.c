
#include <log.h>
#include <renderer.h>
#include <window.h>

#include <stdlib.h>

void resize_callback(struct window *window, u32 width, u32 height) {
    struct renderer *r = window_get_user_ptr(window);
    renderer_resize(r, width, height);
}

i32 main(void) {
    if (!window_init()) {
        return 1;
    }

    struct window window;
    if (!window_create(&window, 800, 600, "Clouds")) {
        return 1;
    }

    struct renderer *r = calloc(1, sizeof(struct renderer));
    if (!renderer_init(r, &window)) {
        LOGM(ERROR, "failed to init renderer");
        return 1;
    }

    window_set_user_ptr(&window, r);
    window_set_resize_callback(&window, resize_callback);

    f32 last_time = window_get_time();
    while (!window_should_close(&window)) {
        f32 current_time = window_get_time();
        f32 dt = current_time - last_time;
        last_time = current_time;

        renderer_update(r, &window, dt);
        renderer_draw(r, &window);
    }

    renderer_deint(r);
    free(r);

    window_destroy(&window);
    window_deinit();

    return 0;
}
