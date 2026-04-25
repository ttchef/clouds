
#include <cmath.h>

// this file may only be included
// in .c files

// TODO move out into a good file idk
struct model_color_pc {
    matrix model;
    vec4 cam_pos;
    vec4 color;
};

struct model_texture_pc {
    matrix model;
    vec4 cam_pos;
    u32 texture_index;
};

struct cloud_pc {
    matrix model;
    vec4 cam_pos;
    vec4 color;
    float time;
};

struct shadow_pc {
    matrix model;
    matrix light_space;
};
