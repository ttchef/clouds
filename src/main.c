
#include <log.h>
#include <renderer.h>
#include <window.h>

#include <stdlib.h>

i32 main(void) {
    if (!window_init()) {
        return 1;
    }

    struct window window;
    if (!window_create(&window, 800, 600, "Clouds")) {
        return 1;
    }

    struct renderer *r = calloc(sizeof(struct renderer), 1);
    if (!renderer_init(r, &window)) {
        LOGM(ERROR, "failed to init renderer");
        return 1;
    }

    while (!window_should_close(&window)) {
    }

    renderer_deint(r);
    free(r);

    window_destroy(&window);
    window_deinit();

    return 0;
}
