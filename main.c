
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan_core.h>

#include "renderer.h"

struct context {
    GLFWwindow *window;
    struct rcontext rctx;
};

i32 main(void) {
    struct context ctx;

    if (!glfwInit()) {
        fprintf(stderr, "failed to init glfw\n");
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    ctx.window = glfwCreateWindow(800, 600, "Clouds", NULL, NULL);
    if (!ctx.window) {
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

    renderer_init(&ctx.rctx, ctx.window, n_exts, exts, 1, layers);

    while (glfwWindowShouldClose(ctx.window)) {

        glfwPollEvents();
    }

    renderer_deint(&ctx.rctx);

    glfwDestroyWindow(ctx.window);
    glfwTerminate();

    return 0;
}
