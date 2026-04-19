

#ifndef CMAHTH_H
#define CMAHTH_H

#include "types.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define RAD2DEG(x) (x * 180.0f / M_PI)
#define DEG2RAD(x) (x * M_PI / 180.0f)

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct {
    f32 x;
    f32 y;
} vec2;

typedef struct {
    i32 x;
    i32 y;
} vec2i;

typedef struct {
    f32 x;
    f32 y;
    f32 z;
} vec3;

typedef struct {
    f32 x;
    f32 y;
    f32 z;
    f32 w;
} vec4;

typedef struct {
    f32 m[16];
} matrix;

f32 math_clamp(f32 n, f32 lower, f32 upper);
int math_clampi(int n, int lower, int upper);

vec2 math_vec2_add(vec2 a, vec2 b);
vec2 math_vec2_subtract(vec2 a, vec2 b);
vec2 math_vec2_scale(vec2 v, f32 scalar);
vec2 math_vec2_rotate(vec2 v, f32 angle);

f32 math_vec2_length(vec2 v);
vec2 math_vec2_norm(vec2 v);
f32 math_vec2_distance(vec2 a, vec2 b);
f32 math_vec2_dot(vec2 a, vec2 b);
f32 math_vec2_angle_cos(vec2 a, vec2 b);

/* Angle in degrees */
f32 math_vec2_angle(vec2 a, vec2 b);
vec2 math_vec2_mul_matrix(vec2 v, matrix *m);

vec2i math_vec2_to_vec2i(vec2 v);
vec2 math_vec2i_to_vec2(vec2i v);

vec3 math_vec3_add(vec3 a, vec3 b);
vec3 math_vec3_subtract(vec3 a, vec3 b);
vec3 math_vec3_mul(vec3 a, vec3 b); // wedge product
vec3 math_vec3_scale(vec3 v, f32 scalar);
f32 math_vec3_length(vec3 v);
vec3 math_vec3_norm(vec3 v);
vec3 math_vec3_cross(vec3 a, vec3 b);

matrix math_matrix_identity();
matrix math_matrix_translate(f32 x, f32 y, f32 z);
matrix math_matrix_scale(f32 x, f32 y, f32 z);

/* Angle in degrees */
matrix math_matrix_rotate_2d(f32 angle);
matrix math_matrix_mul(const matrix a, const matrix b);

/* utility */
vec3 math_vec3_from_vec4(vec4 v);
vec4 math_vec4_from_vec3(vec3 v, f32 w);

void math_vec2_print(vec2 v);
void math_vec3_print(vec3 v);
void math_vec4_print(vec4 v);
void math_matrix_print(matrix *m);

// right handed
matrix math_matrix_orthographic(f32 left, f32 right, f32 bottom, f32 top,
                                f32 near, f32 far);
matrix math_matrix_perspective(f32 fov, f32 aspect, f32 near, f32 far);
matrix math_matrix_look_at(vec3 eye, vec3 target, vec3 up);

#endif // CMAHTH_H
