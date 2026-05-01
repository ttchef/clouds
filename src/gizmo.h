
#ifndef GIZMO_H
#define GIZMO_H

#include <cmath.h>
#include <types.h>

enum {
    GIZMO_AXIS_NONE,
    GIZMO_AXIS_X,
    GIZMO_AXIS_Y,
    GIZMO_AXIS_Z,
};

struct gizmo_hitbox {
    vec3 start;
    vec3 end;
    f32 radius;
    i32 axis;
};

struct gizmo {
    struct gizmo_hitbox hitboxes[3];
    i32 active_axis;
    vec3 drag_start;
    vec3 draw_plane_normal;
};

#endif // GIZMO_H
