

#include "cmath.h"
#include "renderer.h"

#include <stdio.h>

f32 math_clamp(f32 n, f32 lower, f32 upper) {
    if (n < lower)
        return lower;
    if (n > upper)
        return upper;
    return n;
}

i32 math_clampi(i32 n, i32 lower, i32 upper) {
    if (n < lower) {
        return lower;
    }
    if (n > upper) {
        return upper;
    }
    return n;
}

vec2 math_vec2_add(vec2 a, vec2 b) { return (vec2){a.x + b.x, a.y + b.y}; }

vec2 math_vec2_subtract(vec2 a, vec2 b) { return (vec2){a.x - b.x, a.y - b.y}; }

vec2 math_vec2_scale(vec2 v, f32 scalar) {
    return (vec2){v.x * scalar, v.y * scalar};
}

vec2 math_vec2_rotate(vec2 v, f32 angle) {
    f32 c = cosf(angle);
    f32 s = sinf(angle);
    return (vec2){
        .x = v.x * c - v.y * s,
        .y = v.x * s + v.y * c,
    };
}

f32 math_vec2_length(vec2 v) { return sqrtf(v.x * v.x + v.y * v.y); }

vec2 math_vec2_norm(vec2 v) {
    f32 len = math_vec2_length(v);

    if (len == 0.0f) { /* if that even works with f32  precision */
        return (vec2){0, 0};
    }

    return (vec2){
        .x = v.x / len,
        .y = v.y / len,
    };
}

f32 math_vec2_distance(vec2 a, vec2 b) {
    vec2 delta = math_vec2_subtract(a, b);
    return math_vec2_length(delta);
}

f32 math_vec2_dot(vec2 a, vec2 b) { return a.x * b.x + a.y * b.y; }

f32 math_vec2_angle_cos(vec2 a, vec2 b) {
    return math_vec2_dot(a, b) / (math_vec2_length(a) * math_vec2_length(b));
}

f32 math_vec2_angle(vec2 a, vec2 b) { return acosf(math_vec2_angle_cos(a, b)); }

vec2 math_vec2_mul_matrix(vec2 vec, matrix *m) {

    vec4 v = (vec4){
        vec.x,
        vec.y,
        0.0f,
        0.0f,
    };

    vec2 r;

    r.x = m->m[0] * v.x + m->m[4] * v.y + m->m[8] * v.z + m->m[12] * v.w;
    r.y = m->m[1] * v.x + m->m[5] * v.y + m->m[9] * v.z + m->m[13] * v.w;

    return r;
}

vec2i math_vec2_to_vec2i(vec2 v) { return (vec2i){(int)v.x, (int)v.y}; }

vec2 math_vec2i_to_vec2(vec2i v) {
    return (vec2){
        (f32)v.x,
        (f32)v.y,
    };
}

void math_matrix_identity(matrix *m) {
    *m = (matrix){.m = {[0] = 1.0f, [5] = 1.0f, [10] = 1.0f, [15] = 1.0f}};
}

void math_matrix_translate(matrix *m, const f32 x, const f32 y, const f32 z) {
    math_matrix_identity(m);
    m->m[12] = x;
    m->m[13] = y;
    m->m[14] = z;
}

void math_matrix_scale(matrix *m, const f32 x, const f32 y, const f32 z) {
    *m = (matrix){.m = {[0] = x, [5] = y, [10] = z, [15] = 1.0f}};
}

void math_matrix_rotate_2d(matrix *m, f32 angle) {
    f32 theta = DEG2RAD(angle);

    *m = (matrix){.m = {
                      [0] = cos(theta),
                      [1] = -sin(theta),
                      [4] = sin(theta),
                      [5] = cos(theta),
                      [10] = 1.0f,
                      [15] = 1.0f,
                  }};
}

/* TODO: probaly optimize or smth */
void math_matrix_mul(matrix *out, matrix *a, matrix *b) {
    for (i32 r = 0; r < 4; r++) {
        for (i32 c = 0; c < 4; c++) {
            f32 sum = 0.0f;
            for (i32 k = 0; k < 4; k++) {
                sum += a->m[k * 4 + r] * b->m[c * 4 + k];
            }
            out->m[c * 4 + r] = sum;
        }
    }
}

void math_vec2_print(vec2 v) { printf("Vec2: (%.4f, %.4f)\n", v.x, v.y); }

void math_vec4_print(vec4 v) {
    printf("Vec4: (%.4f, %.4f, %.4f, %.4f)\n", v.x, v.y, v.z, v.w);
}

void math_matrix_print(matrix *m) {
    printf("Matrix:\n");
    for (i32 i = 0; i < 4; i++) {
        printf("\tRow %d: [%.4f, %.4f, %.4f, %.4f]\n", i + 1, m->m[i + 0],
               m->m[i + 1], m->m[i + 2], m->m[i + 3]);
    }
}

void math_matrix_orthographic(matrix *m, f32 left, f32 right, f32 bottom,
                              f32 top, f32 near, f32 far) {
    *m = (matrix){.m = {
                      [0] = 2.0f / (right - left),
                      [5] = 2.0f / (top - bottom),
                      [10] = 2.0f / (near - far),
                      [15] = 1.0f,
                      [12] = (left + right) / (left - right),
                      [13] = (bottom + top) / (bottom - top),
                      [14] = (near + far) / (near - far),
                  }};
}

void math_matrix_get_orthographic(u32 w, u32 h, matrix *m) {
    math_matrix_orthographic(m, 0.0f, w, h, 0.0f, -1.0f, 1.0f);
}

void math_matrix_get_perspective(f32 fov, f32 aspect, f32 near, f32 far,
                                 matrix *m) {
    f32 fov_scale = 1.0f / tanf(fov / 2.0);

    *m = (matrix){.m = {
                      -fov_scale / aspect,
                      0.0f,
                      0.0f,
                      0.0f,
                      0.0f,
                      fov_scale,
                      0.0f,
                      0.0f,
                      0.0f,
                      0.0f,
                      far / (near - far),
                      -1.0f,
                      0.0f,
                      0.0f,
                      (far * near) / (near - far),
                      0.0f,
                  }};
}
