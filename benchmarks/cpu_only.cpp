#include "zerolog/logger.hpp"
#include <benchmark/benchmark.h>

static void BM_ZeroLog_CPU(benchmark::State& state) {
    zerolog::Logger<zerolog::NullSink> logger(zerolog::NullSink{}, false);
    for (auto _ : state) {
        logger.info("Static message");
    }
}
BENCHMARK(BM_ZeroLog_CPU);
BENCHMARK_MAIN();
