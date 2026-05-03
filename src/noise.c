
#include <cmath.h>
#include <log.h>
#include <math.h>
#include <noise.h>

#include <FastNoiseLite/FastNoiseLite.h>

#include <stdlib.h>

#define OCTAVES_WORLY 3
#define OCTAVES_PERLIN 4

// how much worly noise (1 all worly; 0 all perlin)
#define WEIGHT 0.5f

static f32 sample_tiling_3d(fnl_state *noise, u32 x, u32 y, u32 z, u32 size) {
    f32 s = (f32)size;

    f32 x1 = (f32)x;
    f32 y1 = (f32)y;
    f32 z1 = (f32)z;

    f32 x2 = x1 - s;
    f32 y2 = y1 - s;
    f32 z2 = z1 - s;

    f32 wx = x1 / s;
    f32 wy = y1 / s;
    f32 wz = z1 / s;

    wx = wx * wx * (3.0f - 2.0f * wx);
    wy = wy * wy * (3.0f - 2.0f * wy);
    wz = wz * wz * (3.0f - 2.0f * wz);

    f32 v000 = fnlGetNoise3D(noise, x1, y1, z1);
    f32 v100 = fnlGetNoise3D(noise, x2, y1, z1);
    f32 v010 = fnlGetNoise3D(noise, x1, y2, z1);
    f32 v110 = fnlGetNoise3D(noise, x2, y2, z1);
    f32 v001 = fnlGetNoise3D(noise, x1, y1, z2);
    f32 v101 = fnlGetNoise3D(noise, x2, y1, z2);
    f32 v011 = fnlGetNoise3D(noise, x1, y2, z2);
    f32 v111 = fnlGetNoise3D(noise, x2, y2, z2);

    f32 v00 = v000 + wx * (v100 - v000);
    f32 v10 = v010 + wx * (v110 - v010);
    f32 v01 = v001 + wx * (v101 - v001);
    f32 v11 = v011 + wx * (v111 - v011);

    f32 v0 = v00 + wy * (v10 - v00);
    f32 v1 = v01 + wy * (v11 - v01);

    f32 val = v0 + wz * (v1 - v0);

    return (val + 1.0f) * 0.5f;
}

static f32 sample_tiling_2d(fnl_state *noise, u32 x, u32 y, u32 size) {
    f32 s = (f32)size;

    f32 x1 = (f32)x;
    f32 y1 = (f32)y;
    f32 x2 = x1 - s;
    f32 y2 = y1 - s;

    f32 wx = x1 / s;
    f32 wy = y1 / s;

    wx = wx * wx * (3.0f - 2.0f * wx);
    wy = wy * wy * (3.0f - 2.0f * wy);

    f32 v00 = fnlGetNoise2D(noise, x1, y1);
    f32 v10 = fnlGetNoise2D(noise, x2, y1);
    f32 v01 = fnlGetNoise2D(noise, x1, y2);
    f32 v11 = fnlGetNoise2D(noise, x2, y2);

    f32 v0 = v00 + wx * (v10 - v00);
    f32 v1 = v01 + wx * (v11 - v01);

    f32 val = v0 + wy * (v1 - v0);

    return (val + 1.0f) * 0.5f;
}

void noise_gen_fbm_worly(f32 *data, u32 size, u32 dimensions) {
    if (dimensions != NOISE_TYPE_2D && dimensions != NOISE_TYPE_3D) {
        LOGM(ERROR, "invalid noise generation dimensions: %d", dimensions);
        data = NULL;
        return;
    }

    fnl_state noise = fnlCreateState();
    noise.noise_type = FNL_NOISE_CELLULAR;
    noise.cellular_distance_func = FNL_CELLULAR_DISTANCE_EUCLIDEAN;
    noise.cellular_return_type = FNL_CELLULAR_RETURN_TYPE_DISTANCE;

    u32 index = 0;
    for (u32 z = 0; z < ((dimensions == NOISE_TYPE_3D) ? size : 1); z++) {
        for (u32 y = 0; y < size; y++) {
            for (u32 x = 0; x < size; x++) {
                f32 total = 0.0f;
                f32 frqeuency = 0.073f;
                f32 amplitude = 0.9f;
                f32 persistence = 0.3f;

                for (u32 o = 0; o < OCTAVES_WORLY; o++) {
                    noise.frequency = frqeuency;

                    f32 val;
                    if (dimensions == NOISE_TYPE_3D) {
                        val = sample_tiling_3d(&noise, x, y, z, size);
                    } else {
                        val = sample_tiling_2d(&noise, x, y, size);
                    }

                    val = 1.0f - val;
                    val -= 0.2f;
                    val *= amplitude;

                    if (val < 0.0f) {
                        val = 0.0f;
                    }
                    total += val;
                    frqeuency *= 2.0f;
                    amplitude *= persistence;
                }
                if (total > 1.0f) {
                    total = 1.0f;
                }
                data[index++] = total;
            }
        }
    }
}

void noise_gen_fbm_perlin(f32 *data, u32 size, u32 dimensions) {
    if (dimensions != NOISE_TYPE_2D && dimensions != NOISE_TYPE_3D) {
        LOGM(ERROR, "invalid noise generation dimensions: %d", dimensions);
        data = NULL;
        return;
    }

    fnl_state noise = fnlCreateState();
    noise.noise_type = FNL_NOISE_PERLIN;
    noise.frequency = 0.01f;

    u32 index = 0;
    for (u32 z = 0; z < ((dimensions == NOISE_TYPE_3D) ? size : 1); z++) {
        for (u32 y = 0; y < size; y++) {
            for (u32 x = 0; x < size; x++) {
                f32 total = 0.0f;
                f32 frqeuency = 0.06f;
                f32 amplitude = 0.85f;
                f32 persistence = 0.47f;

                for (u32 o = 0; o < OCTAVES_PERLIN; o++) {
                    noise.frequency = frqeuency;

                    f32 val;
                    if (dimensions == NOISE_TYPE_3D) {
                        val = sample_tiling_3d(&noise, x, y, z, size);
                    } else {
                        val = sample_tiling_2d(&noise, x, y, size);
                    }

                    val -= 0.1f;
                    val *= amplitude;

                    if (val < 0.0f) {
                        val = 0.0f;
                    }
                    total += val;
                    frqeuency *= 2.0f;
                    amplitude *= persistence;
                }
                if (total > 1.0f) {
                    total = 1.0f;
                }
                total -= 0.15f;
                if (total < 0.0f) {
                    total = 0.0f;
                }
                data[index++] = total;
            }
        }
    }
}

void noise_gen_perlin_worly(f32 *data, u32 size, u32 dimensions) {
    if (dimensions != NOISE_TYPE_2D && dimensions != NOISE_TYPE_3D) {
        LOGM(ERROR, "invalid noise generation dimensions: %d", dimensions);
        data = NULL;
        return;
    }

    f32 *worly;
    f32 *perlin;

    if (dimensions == NOISE_TYPE_3D) {
        worly = malloc(size * size * size * sizeof(f32));
        perlin = malloc(size * size * size * sizeof(f32));
    } else {
        worly = malloc(size * size * sizeof(f32));
        perlin = malloc(size * size * sizeof(f32));
    }

    noise_gen_fbm_worly(worly, size, dimensions);
    noise_gen_fbm_perlin(perlin, size, dimensions);

    u32 index = 0;
    for (u32 z = 0; z < ((dimensions == NOISE_TYPE_3D) ? size : 1); z++) {
        for (u32 y = 0; y < size; y++) {
            for (u32 x = 0; x < size; x++) {
                u32 i = (z * size * size) + (y * size) + x;
                f32 val = (1.0f - WEIGHT) * perlin[i] + WEIGHT * worly[i];
                data[index++] = val;
            }
        }
    }

    free(worly);
    free(perlin);
}
