
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

    model_id boom_box = model_create_file(r, "assets/models/BoomBox.glb");
    model_id torus = model_create_file(r, "assets/models/torus.glb");
    model_id logo = model_create_file(r, "assets/models/logo.glb");
    model_id glibglob = model_create_file(r, "assets/models/Gernade.glb");

    // TODO: support multiple meshes
    // model_id zig =
    //  renderer_create_model(&c->rctx, "assets/models/zthunder.glb");

    texture_id wood = texture_create_file(r, "assets/textures/wood.png");
    texture_id brick = texture_create_file(r, "assets/textures/brickwall.png");

    model_set_texture(r, torus, brick);

    light_id spot =
        light_spot_create(r, (vec3){0, 1, -1}, (vec3){0, -0.5, -0.5},
                          (vec3){0.7, 0.2, 0.6}, 150.0f, 12.5f, 17.5f);

    // light_id dir = renderer_create_dir_light(&c->rctx, (vec3){0, -0.5, -0.5},
    // (vec3){1.0, 0.0, 0.0});

    f32 last_time = window_get_time();
    while (!window_should_close(&window)) {
        f32 current_time = window_get_time();
        f32 dt = current_time - last_time;
        last_time = current_time;

        renderer_update(r, &window, dt);

        f32 red = (sin(window_get_time()) + 1) * 0.5f;
        f32 green = (sin(window_get_time() + 1.0f) + 1) * 0.5f;
        f32 blue = (sin(window_get_time() + 2.0f) + 1) * 0.5f;

        light_spot_update(r, spot, r->camera.pos, r->camera.direction,
                          (vec3){red, green, blue}, 20.0f, 12.5f, 17.5f);

        draw_box(r, (vec3){0.0, -1.5, 0}, (vec3){10, 1, 10},
                 (vec4){0.2, 0.5, 0.8, 1.0}, wood);

        draw_box(r, (vec3){0.3, 0.0, -1.5f}, (vec3){0.2, 0.2, 0.2},
                 (vec4){0.0, 1.0, 0.0, 1.0}, NO_TEXTURE);

        draw_box(r, (vec3){-0.3, 0.0, -1.5f}, (vec3){0.2, 0.2, 0.2},
                 (vec4){0.0, 1.0, 0.0, 1.0}, NO_TEXTURE);

        draw_model_texture(r, (vec3){0, 1.5f, 0}, (vec3){1, 1, 1}, torus);

        draw_model_texture(r, (vec3){0.0, 0.0, -3.0f}, (vec3){10, 10, 10},
                           boom_box);

        draw_model_texture(r, (vec3){0.0, 3, -3.0f}, (vec3){10, 10, 10}, logo);
        draw_model_texture(r, (vec3){4.0, 3, -3.0f}, (vec3){1, 1, 1}, glibglob);

        draw_cloud(r, (vec3){0, 4, 0}, (vec3){1, 1, 1},
                   (vec4){0.0f, 1.0f, 0.0f, 1.0f});

        renderer_draw(r, &window);
        window_poll_events();
    }

    renderer_deint(r);
    free(r);

    window_destroy(&window);
    window_deinit();

    return 0;
}
