
#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

#define FRAMES_IN_FLIGHT 3

#define ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))

#include <cmath.h>

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

#endif // TYPES_H
