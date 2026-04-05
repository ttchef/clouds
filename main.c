
#include <stdio.h>
#include <stdlib.h>

#include "renderer.h"

struct context {};

int32_t main(void) {
    if (!glfwInit()) {
        fprintf(stderr, "failed to init glfw\n");
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(800, 600, "Clouds", NULL, NULL);

    struct rcontext rctx;
    renderer_init(&rctx);

    while (glfwWindowShouldClose(window)) {

        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
