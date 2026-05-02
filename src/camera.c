
#include "cloud.h"
#include "cmath.h"
#include "gizmo.h"
#include <camera.h>
#include <collision.h>
#include <log.h>
#include <renderer.h>

#include <GLFW/glfw3.h>

void camera_init(struct camera *cam) {
    cam->pos = (vec3){0.0f, 0.0f, 0.0f};
    cam->direction = (vec3){0.0f, 0.0f, -1.0f};
    cam->speed = 5.0f;
    cam->sensitivity = 0.15f;
}

void camera_update(struct renderer *r, struct camera *cam,
                   struct window *window, f32 dt) {
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

    if (glfwGetMouseButton(window->handle, GLFW_MOUSE_BUTTON_RIGHT) ==
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

    if (glfwGetMouseButton(window->handle, GLFW_MOUSE_BUTTON_RIGHT) ==
        GLFW_RELEASE) {
        glfwSetInputMode(window->handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        cam->invis_cursor = false;
    }

    bool left_mouse_now =
        glfwGetMouseButton(window->handle, GLFW_MOUSE_BUTTON_LEFT) ==
        GLFW_PRESS;

    if (left_mouse_now && !cam->left_mouse_last) {
        matrix inverse_view = math_matrix_inverse(r->matrix_ubo.data.view);
        matrix inverse_proj = math_matrix_inverse(r->matrix_ubo.data.proj);

        vec2 mouse = window_get_mouse_pos(window);
        vec3 ray_dir = collision_screen_to_ray(
            mouse, window->width, window->height, inverse_view, inverse_proj);

        struct cloud_manager *m = &r->cloud_manager;

        if (m->selected_cloud != NO_CLOUD) {
            struct cloud *c = &m->clouds[m->selected_cloud];
            struct gizmo *g = &c->gizmo;

            g->active_axis = GIZMO_AXIS_NONE;

            f32 best = 1e9f;

            for (u32 i = 0; i < 3; i++) {
                f32 t;

                if (collision_ray_capsule_intersect(
                        cam->pos, ray_dir, g->hitboxes[i].start,
                        g->hitboxes[i].end, g->hitboxes[i].radius, &t)) {
                    if (t < best) {
                        best = t;
                        g->active_axis = g->hitboxes[i].axis;
                        LOGM(HIGHLITE, "Collision: %d", g->active_axis);
                    }
                }
            }

            if (g->active_axis != GIZMO_AXIS_NONE) {
                g->drag_start =
                    math_vec3_add(cam->pos, math_vec3_scale(ray_dir, best));

                vec3 axis = {
                    .x = g->active_axis == GIZMO_AXIS_X ? 1.0f : 0.0f,
                    .y = g->active_axis == GIZMO_AXIS_Y ? 1.0f : 0.0f,
                    .z = g->active_axis == GIZMO_AXIS_Z ? 1.0f : 0.0f,
                };

                f32 proj = math_vec3_dot(cam->direction, axis);
                vec3 normal = math_vec3_subtract(cam->direction,
                                                 math_vec3_scale(axis, proj));

                g->draw_plane_normal = math_vec3_norm(normal);

                goto done;
            }
        }

        f32 t;
        for (u32 i = 0; i < MAX_CLOUDS; i++) {
            struct cloud *c = &m->clouds[i];
            if (!c->valid)
                continue;

            struct aabb box = collision_get_aabb(c->pos, c->scale);

            if (collision_ray_aabb_intersect(cam->pos, ray_dir, box, &t)) {
                if (m->selected_cloud == (i32)i) {
                    m->selected_cloud = NO_CLOUD;
                } else {
                    m->selected_cloud = i;
                }
                break;
            }
        }

    done:
    }

    if (left_mouse_now && cam->left_mouse_last) {
        struct cloud_manager *m = &r->cloud_manager;

        if (m->selected_cloud != NO_CLOUD) {
            struct cloud *c = &m->clouds[m->selected_cloud];
            struct gizmo *g = &c->gizmo;

            if (g->active_axis != GIZMO_AXIS_NONE) {
                LOGM(HIGHLITE, "DRAG");

                vec3 axis = {
                    .x = g->active_axis == GIZMO_AXIS_X ? 1.0f : 0.0f,
                    .y = g->active_axis == GIZMO_AXIS_Y ? 1.0f : 0.0f,
                    .z = g->active_axis == GIZMO_AXIS_Z ? 1.0f : 0.0f,
                };

                // TODO: optimize so no need to recompute these
                matrix inverse_view =
                    math_matrix_inverse(r->matrix_ubo.data.view);
                matrix inverse_proj =
                    math_matrix_inverse(r->matrix_ubo.data.proj);

                vec2 mouse = window_get_mouse_pos(window);
                vec3 ray_dir = collision_screen_to_ray(
                    mouse, window->width, window->height, inverse_view,
                    inverse_proj);

                f32 t;
                if (collision_ray_plane_intersect(cam->pos, ray_dir, c->pos,
                                                  g->draw_plane_normal, &t)) {
                    vec3 hit =
                        math_vec3_add(cam->pos, math_vec3_scale(ray_dir, t));

                    vec3 diff = math_vec3_subtract(hit, g->drag_start);
                    f32 delta = math_vec3_dot(diff, axis);

                    c->pos =
                        math_vec3_add(c->pos, math_vec3_scale(axis, delta));

                    g->drag_start = math_vec3_add(g->drag_start,
                                                  math_vec3_scale(axis, delta));

                    cloud_update_gizmo(r, m->selected_cloud);
                }
            }
        }
    }

    cam->left_mouse_last = left_mouse_now;

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
