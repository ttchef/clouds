
#include <log.h>
#include <window.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vk/init.h>

static void error_callback(i32 error, const char *description) {
    LOGM(ERROR, "glfw error (%d): %s", error, description);
}

void glfw_resize_callback(GLFWwindow *glfw_window, i32 w, i32 h) {
    struct window *window = glfwGetWindowUserPointer(glfw_window);
    if (window->resize) {
        window->resize(window, (u32)w, (u32)h);
    } else {
        LOGM(WARN, "window resize but no resize callback set");
    }

    window->width = (u32)w;
    window->height = (u32)h;
}

bool window_init(void) {
    glfwSetErrorCallback(error_callback);

    // X11 for renderdoc
    // glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    if (!glfwInit()) {
        LOGM(ERROR, "failed to init glfw");
        return false;
    }

    return true;
}

bool window_create(struct window *window, u32 width, u32 height,
                   const char *title) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window->handle =
        glfwCreateWindow((i32)width, (i32)height, title, NULL, NULL);
    if (!window->handle) {
        LOGM(ERROR, "failed to create glfw window");
        return false;
    }

    glfwSetWindowUserPointer(window->handle, window);
    glfwSetWindowSizeCallback(window->handle, glfw_resize_callback);

    window->width = width;
    window->height = height;

    return true;
}

void window_destroy(struct window *window) {
    glfwDestroyWindow(window->handle);
}

void window_deinit(void) { glfwTerminate(); }

bool window_should_close(struct window *window) {
    return glfwWindowShouldClose(window->handle);
}

void window_create_surface(struct window *window, struct vk_init *init) {
    glfwCreateWindowSurface(init->instance, window->handle, NULL,
                            &init->surface);
}

const char **window_get_instance_exts(u32 *n_exts) {
    const char **exts = glfwGetRequiredInstanceExtensions(n_exts);
    if (!exts) {
        *n_exts = 0;
    }

    return exts;
}

f32 window_get_time() { return glfwGetTime(); }

void window_set_resize_callback(struct window *window,
                                window_resize_callback_pfn func) {
    window->resize = func;
}

void window_set_user_ptr(struct window *window, void *ptr) {
    window->user_ptr = ptr;
}

void *window_get_user_ptr(struct window *window) { return window->user_ptr; }

void window_poll_events() { glfwPollEvents(); }
