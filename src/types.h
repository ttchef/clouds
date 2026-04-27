
#ifndef TYPES_H
#define TYPES_H

#define _POSIX_C_SOURCE 200809L

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

struct model_color_pc;
struct model_texture_pc;
struct cloud_pc;
struct shadow_pc;

#endif // TYPES_H
