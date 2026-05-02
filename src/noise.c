
#include <log.h>
#include <noise.h>

#include <FastNoiseLite/FastNoiseLite.h>

#include <stdlib.h>

#define OCTAVES_WORLY 3
#define OCTAVES_PERLIN 4

// how much worly noise (1 all worly; 0 all perlin)
#define WEIGHT 0.5f

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
                        val = (fnlGetNoise3D(&noise, x, y, z) + 1.0f) * 0.5f;
                    } else {
                        val = (fnlGetNoise2D(&noise, x, y) + 1.0f) * 0.5f;
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
                        val = (fnlGetNoise3D(&noise, x, y, z) + 1.0f) * 0.5f;
                    } else {
                        val = (fnlGetNoise2D(&noise, x, y) + 1.0f) * 0.5f;
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
