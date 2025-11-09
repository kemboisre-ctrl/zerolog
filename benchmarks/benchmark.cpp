#include "zerolog/logger.hpp"
#include <benchmark/benchmark.h>
#include <atomic>
#include <thread>

using namespace zerolog;

// Single-threaded synchronous baseline
static void BM_ZeroLog_Sync(benchmark::State& state) {
    Logger<NullSink> logger(NullSink{}, false);
    
    for (auto _ : state) {
        logger.info("Test message {}", state.iterations());
    }
}
BENCHMARK(BM_ZeroLog_Sync);

// Single-threaded asynchronous
static void BM_ZeroLog_Async_ST(benchmark::State& state) {
    Logger<NullSink> logger(NullSink{}, true);
    
    for (auto _ : state) {
        logger.info("Test message {}", state.iterations());
    }
    
    logger.flush();
}
BENCHMARK(BM_ZeroLog_Async_ST);

// Multi-threaded: measures per-thread latency under contention
static void BM_ZeroLog_Async_MT(benchmark::State& state) {
    Logger<NullSink> logger(NullSink{}, true);
    const int num_threads = 4;
    
    state.counters["threads"] = num_threads;
    
    for (auto _ : state) {
        std::vector<std::thread> threads;
        std::atomic<int64_t> total_msgs{0};
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&logger, &total_msgs, i] {
                // Each thread logs 100 messages
                for (int j = 0; j < 100; ++j) {
                    logger.info("Thread {} message {}", i, j);
                    total_msgs.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        
        for (auto& t : threads) t.join();
        
        // Report per-message latency
        state.counters["msgs_per_op"] = total_msgs.load();
    }
    
    logger.flush();
}
BENCHMARK(BM_ZeroLog_Async_MT)->UseRealTime();

BENCHMARK_MAIN();
