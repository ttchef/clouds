
#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <types.h>

struct window;

struct benchmark {
    f32 last_time;
    f32 delta_time;

    f32 accumelated_time;
};

void benchmark_init(struct benchmark *benchmark);

f32 benchmark_get_fps(struct benchmark *benchmark);

void benchmark_update(struct benchmark *benchmark, f32 dt);

void benchmark_print(struct benchmark *benchmark);

#endif // BENCHMARK_H
