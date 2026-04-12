
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan_core.h>

#include "log.h"
#include "renderer.h"

struct context {
    GLFWwindow *window;
    struct rcontext rctx;
};

void glfw_resize_callback(GLFWwindow *window, i32 w, i32 h) {
    struct context *c = glfwGetWindowUserPointer(window);
    renderer_resize(&c->rctx, (u32)w, (u32)h);
}

i32 main(void) {
    struct context *c = calloc(sizeof(struct context), 1);

    if (!glfwInit()) {
        fprintf(stderr, "failed to init glfw\n");
        return 1;
    }

    glfwWindowHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    c->window = glfwCreateWindow(800, 600, "Clouds", NULL, NULL);
    if (!c->window) {
        fprintf(stderr, "failed to create window\n");
        glfwTerminate();
        exit(1);
    }

    u32 n_glfw_exts;
    const char **glfw_exts = glfwGetRequiredInstanceExtensions(&n_glfw_exts);

    u32 n_exts = n_glfw_exts + 1;
    const char *exts[n_exts];

    for (u32 i = 0; i < n_glfw_exts; i++) {
        exts[i] = glfw_exts[i];
    }
    exts[n_glfw_exts] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

    const char *layers[] = {
        "VK_LAYER_KHRONOS_validation",
    };

    if (!renderer_init(&c->rctx, c->window, n_exts, exts, 1, layers)) {
        LOGM(ERROR, "failed to init renderer");
        glfwDestroyWindow(c->window);
        glfwTerminate();
        return 1;
    }

    glfwSetWindowUserPointer(c->window, c);
    glfwSetWindowSizeCallback(c->window, glfw_resize_callback);

    model_id boom_box =
        renderer_create_model(&c->rctx, "assets/models/BoomBox.glb");
    model_id torus = renderer_create_model(&c->rctx, "assets/models/torus.glb");
    model_id logo = renderer_create_model(&c->rctx, "assets/models/logo.glb");
    model_id glibglob =
        renderer_create_model(&c->rctx, "assets/models/Gernade.glb");

    // TODO: support multiple meshes
    // model_id zig =
    //  renderer_create_model(&c->rctx, "assets/models/zthunder.glb");

    texture_id wood =
        renderer_create_texture(&c->rctx, "assets/textures/wood.png");
    texture_id brick =
        renderer_create_texture(&c->rctx, "assets/textures/brickwall.png");

    renderer_set_model_texture(&c->rctx, torus, brick);

    // light_id point = renderer_create_point_light(&c->rctx, (vec3){0, 1.5, 0},
    //                                         (vec3){0.7, 0.2, 0.6}, 150.0f);

    light_id spot =
        renderer_create_spot_light(&c->rctx, (vec3){0, 1, 0}, (vec3){0, 0, -1},
                                   (vec3){0.7, 0.2, 0.6}, 150.0f, 12.5f, 17.5f);

    // light_id static_spot =
    //     renderer_create_spot_light(&c->rctx,
    //     (vec3){0, 0, 0},
    //     (vec3){0, 0, -1},
    //                             (vec3){0.7, 0.2, 0.6}, 150.0f, 12.5f, 17.5f);

    light_id dir = renderer_create_dir_light(&c->rctx, (vec3){0, -0.5, -0.5},
                                             (vec3){1.0, 0.0, 0.0});

    f32 last_time = 0.0f;
    while (!glfwWindowShouldClose(c->window)) {
        f32 current_time = glfwGetTime();
        f32 dt = current_time - last_time;
        last_time = current_time;

        renderer_update_cam(&c->rctx, c->window, dt);
        renderer_update(&c->rctx, dt);

        f32 r = (sin(glfwGetTime()) + 1) * 0.5f;
        f32 g = (sin(glfwGetTime() + 1.0f) + 1) * 0.5f;
        f32 b = (sin(glfwGetTime() + 2.0f) + 1) * 0.5f;

        vec3 light_dir = {
            -c->rctx.cam.direction.x,
            -c->rctx.cam.direction.y,
            -c->rctx.cam.direction.z,
        };

        renderer_update_spot_light(&c->rctx, spot, c->rctx.cam.pos, light_dir,
                                   (vec3){r, g, b}, 150.0f, 12.5f, 17.5f);

        renderer_push_box(&c->rctx, (vec3){0.0, -1.5, 0}, (vec3){10, 1, 10},
                          (vec4){0.2, 0.5, 0.8, 1.0}, wood);

        renderer_push_box(&c->rctx, (vec3){0.3, 0.0, -1.5f},
                          (vec3){0.2, 0.2, 0.2}, (vec4){0.0, 1.0, 0.0, 1.0},
                          NO_TEXTURE);

        renderer_push_box(&c->rctx, (vec3){-0.3, 0.0, -1.5f},
                          (vec3){0.2, 0.2, 0.2}, (vec4){0.0, 1.0, 0.0, 1.0},
                          NO_TEXTURE);

        renderer_push_model_texture(&c->rctx, (vec3){0, 1.5f, 0},
                                    (vec3){1, 1, 1}, torus);

        renderer_push_model_texture(&c->rctx, (vec3){0.0, 0.0, -3.0f},
                                    (vec3){10, 10, 10}, boom_box);

        renderer_push_model_texture(&c->rctx, (vec3){0.0, 3, -3.0f},
                                    (vec3){10, 10, 10}, logo);
        renderer_push_model_texture(&c->rctx, (vec3){4.0, 3, -3.0f},
                                    (vec3){1, 1, 1}, glibglob);

        renderer_draw(&c->rctx, c->window);
        glfwPollEvents();
    }

    renderer_deint(&c->rctx);

    glfwDestroyWindow(c->window);
    glfwTerminate();

    free(c);

    return 0;
}
