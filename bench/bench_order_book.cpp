#include <benchmark/benchmark.h>
static void BM_Placeholder(benchmark::State& state) {
    for (auto _ : state) {}
}
BENCHMARK(BM_Placeholder);
