
#include <benchmark.h>
#include <log.h>

// time for next benchmark print
#define BENCHMARK_TIME 1.0

void benchmark_init(struct benchmark *benchmark) {
    benchmark->delta_time = 0.0f;
    benchmark->accumelated_time = 0.0f;
}

f32 benchmark_get_fps(struct benchmark *benchmark) {
    f32 fps = 1.0f / benchmark->delta_time;
    return fps;
}

void benchmark_update(struct benchmark *benchmark, f32 dt) {
    benchmark->delta_time = dt;
    benchmark->accumelated_time += benchmark->delta_time;
};

void benchmark_print(struct benchmark *benchmark) {
    if (benchmark->accumelated_time >= BENCHMARK_TIME) {
        benchmark->accumelated_time = 0.0f;
        LOGM(INFO, "dt: %.4f", benchmark->delta_time);
        LOGM(INFO, "fps: %.2f", benchmark_get_fps(benchmark));
    }
}
