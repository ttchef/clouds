

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

void math_matrix_identity(matrix *m);
void math_matrix_translate(matrix *m, f32 x, f32 y, f32 z);
void math_matrix_scale(matrix *m, f32 x, f32 y, f32 z);

/* Angle in degrees */
void math_matrix_rotate_2d(matrix *m, f32 angle);
void math_matrix_mul(matrix *out, matrix *a, matrix *b);

/* utility */
void math_vec2_print(vec2 v);
void math_vec4_print(vec4 v);
void math_matrix_print(matrix *m);

void math_matrix_orthographic(matrix *m, f32 left, f32 right, f32 bottom,
                              f32 top, f32 near, f32 far);
void math_matrix_get_orthographic(u32 w, u32 h, matrix *m);

#endif // CMAHTH_H
