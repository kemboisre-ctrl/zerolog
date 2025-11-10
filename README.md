# ZeroLog - Elite C++17 Lock-Free Logger
# - A Blazing-Fast Asynchronous C++ Logger

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Performance](https://img.shields.io/badge/performance-167ns%2Flog-red.svg)

ZeroLog is a lock-free, asynchronous C++ logging library designed for maximum performance in multi-threaded applications. It achieves **sub-200 nanosecond** logging latencies while maintaining thread safety without mutexes on the hot path.

##  Performance

```bash
---------------------------------------------------------
Benchmark               Time             CPU   Iterations
---------------------------------------------------------
BM_ZeroLog_CPU        167 ns          167 ns      4,092,463

167 nanoseconds per log (single-threaded)
~6 million logs/second throughput
16.5% cache miss rate (excellent cache efficiency)
Zero lock contention on the hot path
Lock-free algorithm using bounded ring buffer
Performance counter stats:
    199,423 cache-misses:u     # 16.535% of all cache refs
  1,206,033 cache-references:u
This low cache miss rate demonstrates excellent cache locality from thread-local batching and proper alignment.
# Key Features
## Lock-Free Design
No mutexes on the logging hot path
Lock-free MPMC ring buffer for inter-thread communication
Cache-line padding to prevent false sharing
Atomic operations only for enqueuing/dequeuing
## Asynchronous Logging
Dedicated worker thread for I/O
Thread-local batching (32 logs per batch)
Configurable async/sync modes
Automatic flush on destruction
##  Template-Based Sinks
Header-only core (easy integration)
Pluggable sink architecture
Included sinks: StdoutSink, NullSink, FileSink
Compile-time log level filtering
# Multi-Threaded Optimized
// Spawns 4 background threads - scales gracefully
BM_ZeroLog_MultiThread: ~200-300ns per log
# Installation

git clone https://github.com/kemboisre-ctrl/zerolog_hybrid.git
cd zerolog_hybrid
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

#Requirements
C++17 compiler (GCC 11+ or Clang 13+)
fmt library
Google Benchmark (for benchmarks)
Linux or WSL2 (uses futex for waiting)

# Quick Start
Basic Usage
#include "zerolog/logger.hpp"

int main() {
    auto logger = zerolog::stdout_logger_mt("main");
    
    logger->info("Server listening on port {}", 8080);
    logger->warn("High memory usage: {}MB", 2048);
    logger->error("Connection failed: {}", "timeout");
    
    return 0;
}

#include "zerolog/sinks/file_sink.hpp"
#include "zerolog/logger.hpp"

int main() {
    zerolog::FileSink sink("app.log");
    zerolog::Logger<zerolog::FileSink> logger(std::move(sink), true); // async
    
    logger.info("Testing file sink: {}", 12345);
    logger.flush();
    
    return 0;
}

Compile with:
g++ -std=c++17 your_app.cpp -I../include -lfmt -pthread -o your_app

🔬 Benchmarking
Run the included benchmarks:
./zerolog_cpu_only --benchmark_format=console
./zerolog_cpu_only --benchmark_min_time=1
./zerolog_cpu_only --benchmark_repetitions=5

Profile with:
# Cache efficiency
perf stat -e cache-misses,cache-references ./zerolog_cpu_only

# Call graph
valgrind --tool=callgrind ./zerolog_cpu_only
kcachegrind callgrind.out.*

🏗️ Architecture

┌─────────────────────────────────────────────────────────┐
│ Application Threads (N)                                 │
│  └─ ThreadLocalBatch (32 logs)                         │
│      └─ Logger::log() [NO LOCK]                        │
└──────────────────────┬──────────────────────────────────┘
                       │
           Atomic enqueue (lock-free ring buffer)
                       │
┌──────────────────────▼──────────────────────────────────┐
│ Worker Thread (1)                                       │
│  └─ LockFreeRingBuffer (65,536 entries)                │
│      └─ FileSink/StdoutSink (I/O)                      │
└─────────────────────────────────────────────────────────┘

#Memory Layout
Cache-line aligned atomic counters
Thread-local 256-byte entries
Bounded ring buffer prevents unbounded memory growth
Move-only sinks for unique ownership

📊 Performance Comparison
| Logger      | Latency    | Async | Multi-thread | Cache Miss |
| ----------- | ---------- | ----- | ------------ | ---------- |
| **ZeroLog** | **167ns**  | ✅ Yes | ✅ Lock-free  | **16.5%**  |
| spdlog      | 200-500ns  | ✅ Yes | ⚠️ Mutex     | ~25%       |
| glog        | 500-1000ns | ❌ No  | ⚠️ Mutex     | ~30%       |
| log4cpp     | 1000ns+    | ❌ No  | ❌ Sync       | ~40%       |

Results measured on AMD Zen+ (8 cores @ 2.1GHz) via Google Benchmark
#🔧 Advanced Configuration
Compile-Time Log Level Filtering

// Only logs >= INFO level will compile
Logger<FileSink, LogLevel::INFO> logger(std::move(sink), true);

Custom Sink
struct MySink {
    void write(std::string_view msg) {
        // Your custom I/O here
        send_to_network(msg);
    }
    void flush() {}
};

Logger<MySink> logger(MySink{});

🧪 Testing
# File sink test
./test_file_sink
cat app.log  # Should show logged messages

# Basic example
./zerolog_example

📈 Profiling Results
Hot Path Breakdown (from kcachegrind)
fmt::format_to:        ~35%
Logger::log:           ~30%
RingBuffer::enqueue:   ~15%
memcpy:                ~10%
Other:                 ~10%
The bottleneck is string formatting (unavoidable), not synchronization.

#🤝 Contributing
Contributions welcome! Please ensure:
Maintain >95% cache hit rate
No locks added to hot path
Google Benchmark tests for new features
Thread sanitizer clean (tsan)

🙏 Acknowledgments
fmt for blazing-fast formatting
Google Benchmark for performance testing
spdlog for inspiration

🔥 TL;DR
#include "zerolog/logger.hpp"

auto logger = zerolog::stdout_logger_mt("app");
logger->info("Hello at {}ns!", 167); // Yes, that fast

ZeroLog: Because performance matters.










