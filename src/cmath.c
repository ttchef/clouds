

#include "cmath.h"

#include <math.h>
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

f32 math_vec2_len_sqrt(vec2 v) { return v.x * v.x + v.y * v.y; }

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

vec3 math_vec3_add(vec3 a, vec3 b) {
    return (vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

vec3 math_vec3_subtract(vec3 a, vec3 b) {
    return (vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

vec3 math_vec3_mul(vec3 a, vec3 b) {
    return (vec3){a.x * b.x, a.y * b.y, a.z * b.z};
}

vec3 math_vec3_scale(vec3 v, f32 scalar) {
    return (vec3){v.x * scalar, v.y * scalar, v.z * scalar};
}

f32 math_vec3_len_sqrt(vec3 v) { return v.x * v.x + v.y * v.y + v.z * v.z; }

f32 math_vec3_length(vec3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

vec3 math_vec3_norm(vec3 v) {
    f32 len = math_vec3_length(v);

    if (len == 0.0f) {
        return (vec3){0, 0, 0};
    }

    return (vec3){v.x / len, v.y / len, v.z / len};
}

vec3 math_vec3_negate(vec3 v) { return (vec3){-v.x, -v.y, -v.z}; }

f32 math_vec3_dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

vec3 math_vec3_cross(vec3 a, vec3 b) {
    return (vec3){a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                  a.x * b.y - a.y * b.x};
}

vec3 math_vec3_from_vec4(vec4 v) { return (vec3){v.x, v.y, v.z}; }

vec4 math_vec4_from_vec3(vec3 v, f32 w) { return (vec4){v.x, v.y, v.z, w}; }

void math_vec2_print(vec2 v) { printf("Vec2: (%.4f, %.4f)\n", v.x, v.y); }

void math_vec3_print(vec3 v) {
    printf("Vec3: (%.4f, %.4f, %.4f)\n", v.x, v.y, v.z);
}

void math_vec4_print(vec4 v) {
    printf("Vec4: (%.4f, %.4f, %.4f, %.4f)\n", v.x, v.y, v.z, v.w);
}

void math_matrix_print(matrix *m) {
    printf("Matrix:\n");
    for (i32 i = 0; i < 4; i++) {
        printf("\tRow %d: [%.4f, %.4f, %.4f, %.4f]\n", i + 1, m->m[i * 4 + 0],
               m->m[i * 4 + 1], m->m[i * 4 + 2], m->m[i * 4 + 3]);
    }
}

matrix math_matrix_identity(void) {
    matrix m = {0};
    m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
    return m;
}

matrix math_matrix_translate(f32 x, f32 y, f32 z) {
    matrix m = math_matrix_identity();
    m.m[12] = x;
    m.m[13] = y;
    m.m[14] = z;
    return m;
}

matrix math_matrix_scale(f32 x, f32 y, f32 z) {
    matrix m = math_matrix_identity();
    m.m[0] = x;
    m.m[5] = y;
    m.m[10] = z;
    return m;
}

matrix math_matrix_rotate_x(f32 angle) {
    f32 r = DEG2RAD(angle);
    f32 c = cosf(r);
    f32 s = sinf(r);

    matrix m = math_matrix_identity();
    m.m[5] = c;
    m.m[6] = s;
    m.m[9] = -s;
    m.m[10] = c;
    return m;
}

matrix math_matrix_rotate_y(f32 angle) {
    f32 r = DEG2RAD(angle);
    f32 c = cosf(r);
    f32 s = sinf(r);

    matrix m = math_matrix_identity();
    m.m[0] = c;
    m.m[2] = -s;
    m.m[8] = s;
    m.m[10] = c;
    return m;
}

matrix math_matrix_rotate_z(f32 angle) {
    f32 r = DEG2RAD(angle);
    f32 c = cosf(r);
    f32 s = sinf(r);

    matrix m = math_matrix_identity();
    m.m[0] = c;
    m.m[1] = s;
    m.m[4] = -s;
    m.m[5] = c;
    return m;
}

matrix math_matrix_rotate(vec3 axis, f32 angle) {
    f32 r = DEG2RAD(angle);
    f32 c = cosf(r);
    f32 s = sinf(r);
    f32 t = 1.0f - c;

    vec3 a = math_vec3_norm(axis);
    f32 x = a.x, y = a.y, z = a.z;

    matrix m = math_matrix_identity();
    m.m[0] = t * x * x + c;
    m.m[1] = t * x * y + s * z;
    m.m[2] = t * x * z - s * y;

    m.m[4] = t * x * y - s * z;
    m.m[5] = t * y * y + c;
    m.m[6] = t * y * z + s * x;

    m.m[8] = t * x * z + s * y;
    m.m[9] = t * y * z - s * x;
    m.m[10] = t * z * z + c;

    return m;
}

matrix math_matrix_inverse(const matrix m) {
    const f32 *s = m.m;
    matrix out;
    f32 *d = out.m;

    d[0] = s[5] * s[10] * s[15] - s[5] * s[11] * s[14] - s[9] * s[6] * s[15] +
           s[9] * s[7] * s[14] + s[13] * s[6] * s[11] - s[13] * s[7] * s[10];
    d[1] = -s[1] * s[10] * s[15] + s[1] * s[11] * s[14] + s[9] * s[2] * s[15] -
           s[9] * s[3] * s[14] - s[13] * s[2] * s[11] + s[13] * s[3] * s[10];
    d[2] = s[1] * s[6] * s[15] - s[1] * s[7] * s[14] - s[5] * s[2] * s[15] +
           s[5] * s[3] * s[14] + s[13] * s[2] * s[7] - s[13] * s[3] * s[6];
    d[3] = -s[1] * s[6] * s[11] + s[1] * s[7] * s[10] + s[5] * s[2] * s[11] -
           s[5] * s[3] * s[10] - s[9] * s[2] * s[7] + s[9] * s[3] * s[6];

    d[4] = -s[4] * s[10] * s[15] + s[4] * s[11] * s[14] + s[8] * s[6] * s[15] -
           s[8] * s[7] * s[14] - s[12] * s[6] * s[11] + s[12] * s[7] * s[10];
    d[5] = s[0] * s[10] * s[15] - s[0] * s[11] * s[14] - s[8] * s[2] * s[15] +
           s[8] * s[3] * s[14] + s[12] * s[2] * s[11] - s[12] * s[3] * s[10];
    d[6] = -s[0] * s[6] * s[15] + s[0] * s[7] * s[14] + s[4] * s[2] * s[15] -
           s[4] * s[3] * s[14] - s[12] * s[2] * s[7] + s[12] * s[3] * s[6];
    d[7] = s[0] * s[6] * s[11] - s[0] * s[7] * s[10] - s[4] * s[2] * s[11] +
           s[4] * s[3] * s[10] + s[8] * s[2] * s[7] - s[8] * s[3] * s[6];

    d[8] = s[4] * s[9] * s[15] - s[4] * s[11] * s[13] - s[8] * s[5] * s[15] +
           s[8] * s[7] * s[13] + s[12] * s[5] * s[11] - s[12] * s[7] * s[9];
    d[9] = -s[0] * s[9] * s[15] + s[0] * s[11] * s[13] + s[8] * s[1] * s[15] -
           s[8] * s[3] * s[13] - s[12] * s[1] * s[11] + s[12] * s[3] * s[9];
    d[10] = s[0] * s[5] * s[15] - s[0] * s[7] * s[13] - s[4] * s[1] * s[15] +
            s[4] * s[3] * s[13] + s[12] * s[1] * s[7] - s[12] * s[3] * s[5];
    d[11] = -s[0] * s[5] * s[11] + s[0] * s[7] * s[9] + s[4] * s[1] * s[11] -
            s[4] * s[3] * s[9] - s[8] * s[1] * s[7] + s[8] * s[3] * s[5];

    d[12] = -s[4] * s[9] * s[14] + s[4] * s[10] * s[13] + s[8] * s[5] * s[14] -
            s[8] * s[6] * s[13] - s[12] * s[5] * s[10] + s[12] * s[6] * s[9];
    d[13] = s[0] * s[9] * s[14] - s[0] * s[10] * s[13] - s[8] * s[1] * s[14] +
            s[8] * s[2] * s[13] + s[12] * s[1] * s[10] - s[12] * s[2] * s[9];
    d[14] = -s[0] * s[5] * s[14] + s[0] * s[6] * s[13] + s[4] * s[1] * s[14] -
            s[4] * s[2] * s[13] - s[12] * s[1] * s[6] + s[12] * s[2] * s[5];
    d[15] = s[0] * s[5] * s[10] - s[0] * s[6] * s[9] - s[4] * s[1] * s[10] +
            s[4] * s[2] * s[9] + s[8] * s[1] * s[6] - s[8] * s[2] * s[5];

    f32 det = s[0] * d[0] + s[1] * d[4] + s[2] * d[8] + s[3] * d[12];

    if (fabsf(det) < 1e-8f) {
        return math_matrix_identity();
    }

    f32 inv_det = 1.0f / det;
    for (int i = 0; i < 16; i++)
        d[i] *= inv_det;

    return out;
}

matrix math_matrix_mul(const matrix a, const matrix b) {
    matrix res = {0};
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            f32 sum = 0.0f;
            for (int k = 0; k < 4; k++) {
                sum += a.m[k * 4 + r] * b.m[c * 4 + k];
            }
            res.m[c * 4 + r] = sum;
        }
    }
    return res;
}

matrix math_matrix_look_at(vec3 eye, vec3 target, vec3 up) {
    vec3 z = math_vec3_norm(
        (vec3){eye.x - target.x, eye.y - target.y, eye.z - target.z});
    vec3 x = math_vec3_norm(math_vec3_cross(up, z));
    vec3 y = math_vec3_cross(z, x);

    matrix m = {0};
    m.m[0] = x.x;
    m.m[4] = x.y;
    m.m[8] = x.z;
    m.m[1] = y.x;
    m.m[5] = y.y;
    m.m[9] = y.z;
    m.m[2] = z.x;
    m.m[6] = z.y;
    m.m[10] = z.z;
    m.m[12] = -(x.x * eye.x + x.y * eye.y + x.z * eye.z);
    m.m[13] = -(y.x * eye.x + y.y * eye.y + y.z * eye.z);
    m.m[14] = -(z.x * eye.x + z.y * eye.y + z.z * eye.z);
    m.m[15] = 1.0f;
    return m;
}

matrix math_matrix_orthographic(f32 left, f32 right, f32 bottom, f32 top,
                                f32 near, f32 far) {
    matrix m = {0};

    m.m[0] = 2.0f / (right - left);
    m.m[5] = -2.0f / (top - bottom);
    m.m[10] = -1.0f / (far - near);

    m.m[12] = -(right + left) / (right - left);
    m.m[13] = (top + bottom) / (top - bottom);
    m.m[14] = -near / (far - near);

    m.m[15] = 1.0f;

    return m;
}

matrix math_matrix_perspective(f32 fov_deg, f32 aspect, f32 near, f32 far) {
    f32 fov_rad = fov_deg * (3.14159265f / 180.0f);
    f32 f = 1.0f / tanf(fov_rad / 2.0f);

    matrix m = {0};
    m.m[0] = f / aspect;
    m.m[5] = -f;
    m.m[10] = -far / (far - near);
    m.m[11] = -1.0f;
    m.m[14] = -(far * near) / (far - near);
    return m;
}
