
#ifndef CAMERA_H
#define CAMERA_H

#include <cmath.h>
#include <types.h>
#include <window.h>

struct camera {
    f32 speed;
    f32 sensitivity;

    f32 yaw;
    f32 pitch;

    vec2 last_mouse;
    vec3 pos;
    vec3 direction;

    bool invis_cursor;
};

void camera_init(struct camera *cam);

void camera_update(struct camera *cam, struct window *window, f32 dt);

#endif // CAMERA_H
