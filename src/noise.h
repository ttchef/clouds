
#ifndef NOISE_H
#define NOISE_H

#include <types.h>

enum {
    NOISE_TYPE_2D,
    NOISE_TYPE_3D,
};

void noise_gen_fbm_worly(f32 *data, u32 size, u32 dimensions);

void noise_gen_fbm_perlin(f32 *data, u32 size, u32 dimensions);

void noise_gen_perlin_worly(f32 *data, u32 size, u32 dimensions);

#endif // NOISE_H
