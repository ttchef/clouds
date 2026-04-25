
#include <camera.h>

#include <GLFW/glfw3.h>

void camera_init(struct camera *cam) {
    cam->pos = (vec3){0.0f, 0.0f, 0.0f};
    cam->direction = (vec3){0.0f, 0.0f, -1.0f};
    cam->speed = 5.0f;
    cam->sensitivity = 0.15f;
}

void camera_update(struct camera *cam, struct window *window, f32 dt) {
    vec3 forward =
        math_vec3_norm((vec3){cam->direction.x, 0.0f, cam->direction.z});
    vec3 up = {0.0f, 1.0f, 0.0f};
    vec3 right = math_vec3_norm(math_vec3_cross(cam->direction, up));

    if (glfwGetKey(window->handle, GLFW_KEY_W) != GLFW_RELEASE) {
        cam->pos =
            math_vec3_add(cam->pos, math_vec3_scale(forward, cam->speed * dt));
    }
    if (glfwGetKey(window->handle, GLFW_KEY_S) != GLFW_RELEASE) {
        cam->pos = math_vec3_subtract(
            cam->pos, math_vec3_scale(forward, cam->speed * dt));
    }
    if (glfwGetKey(window->handle, GLFW_KEY_A) != GLFW_RELEASE) {
        cam->pos = math_vec3_subtract(cam->pos,
                                      math_vec3_scale(right, cam->speed * dt));
    }
    if (glfwGetKey(window->handle, GLFW_KEY_D) != GLFW_RELEASE) {
        cam->pos =
            math_vec3_add(cam->pos, math_vec3_scale(right, cam->speed * dt));
    }
    if (glfwGetKey(window->handle, GLFW_KEY_SPACE) != GLFW_RELEASE) {
        cam->pos =
            math_vec3_add(cam->pos, math_vec3_scale(up, cam->speed * dt));
    }
    if (glfwGetKey(window->handle, GLFW_KEY_LEFT_SHIFT) != GLFW_RELEASE) {
        cam->pos =
            math_vec3_subtract(cam->pos, math_vec3_scale(up, cam->speed * dt));
    }

    if (glfwGetMouseButton(window->handle, GLFW_MOUSE_BUTTON_LEFT) ==
        GLFW_PRESS) {
        glfwSetInputMode(window->handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        if (!cam->invis_cursor) {
            f64 x, y;
            glfwGetCursorPos(window->handle, &x, &y);

            vec2 pos = (vec2){(f32)x, (f32)y};
            cam->last_mouse = pos;
            cam->invis_cursor = true;
        }
    }

    if (glfwGetMouseButton(window->handle, GLFW_MOUSE_BUTTON_LEFT) ==
        GLFW_RELEASE) {
        glfwSetInputMode(window->handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        cam->invis_cursor = false;
    }

    f64 x, y;
    glfwGetCursorPos(window->handle, &x, &y);

    vec2 pos = (vec2){(f32)x, (f32)y};
    vec2 delta = math_vec2_subtract(pos, cam->last_mouse);

    if (cam->invis_cursor) {
        cam->yaw += delta.x * cam->sensitivity;
        cam->pitch += delta.y * cam->sensitivity;

        if (cam->pitch > 89.0f) {
            cam->pitch = 89.0f;
        } else if (cam->pitch < -89.0f) {
            cam->pitch = -89.0f;
        }

        cam->last_mouse = pos;
    }

    vec3 front;
    front.x = cos(DEG2RAD(cam->pitch)) * sin(DEG2RAD(cam->yaw));
    front.y = -sin(DEG2RAD(cam->pitch));
    front.z = -cos(DEG2RAD(cam->pitch)) * cos(DEG2RAD(cam->yaw));
    cam->direction = math_vec3_norm(front);
}
