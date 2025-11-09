#pragma once
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <fmt/format.h>
#include <chrono>
#include <vector>
#include <cstring>
#include <memory>  // ✅ For std::shared_ptr

namespace zerolog {

enum class LogLevel : uint8_t {
    TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL
};

struct alignas(64) PaddedAtomicSizeT {
    std::atomic<size_t> value{0};
};

class LockFreeRingBuffer {
private:
    static constexpr size_t CACHE_LINE_SIZE = 64;
    alignas(CACHE_LINE_SIZE) PaddedAtomicSizeT head_;
    alignas(CACHE_LINE_SIZE) PaddedAtomicSizeT tail_;
    std::unique_ptr<char[]> buffer_;
    void* aligned_buffer_ = nullptr;
    const size_t buffer_size_;
    const size_t entry_size_;
    const size_t max_entries_;

public:
    explicit LockFreeRingBuffer(size_t entry_size, size_t max_entries)
        : entry_size_(entry_size), max_entries_(max_entries), 
          buffer_size_(entry_size * max_entries) {
        size_t allocation_size = buffer_size_ + 64;
        buffer_ = std::make_unique<char[]>(allocation_size);
        void* ptr = buffer_.get();
        size_t space = allocation_size;
        aligned_buffer_ = std::align(64, buffer_size_, ptr, space);
        if (!aligned_buffer_) {
            throw std::runtime_error("Buffer alignment failed");
        }
    }

    bool try_enqueue(const void* data, size_t len) {
        size_t current_tail = tail_.value.load(std::memory_order_relaxed);
        size_t next_tail;
        int backoff = 0;
        do {
            if ((current_tail - head_.value.load(std::memory_order_acquire)) >= max_entries_) {
                return false;
            }
            next_tail = current_tail + 1;
        } while (!tail_.value.compare_exchange_weak(
            current_tail, next_tail,
            std::memory_order_acq_rel,
            std::memory_order_relaxed));
        
        char* slot = static_cast<char*>(aligned_buffer_) + ((current_tail % max_entries_) * entry_size_);
        memcpy(slot, data, len);
        *reinterpret_cast<uint16_t*>(slot + entry_size_ - 2) = static_cast<uint16_t>(len);
        return true;
    }

    bool try_dequeue(void* data, size_t& len) {
        size_t current_head = head_.value.load(std::memory_order_relaxed);
        if (current_head >= tail_.value.load(std::memory_order_acquire)) {
            return false;
        }
        
        char* slot = static_cast<char*>(aligned_buffer_) + ((current_head % max_entries_) * entry_size_);
        uint16_t slot_len;
        while ((slot_len = *reinterpret_cast<volatile uint16_t*>(slot + entry_size_ - 2)) == 0) {
            std::this_thread::yield();
        }
        
        len = slot_len;
        memcpy(data, slot, len);
        *reinterpret_cast<uint16_t*>(slot + entry_size_ - 2) = 0;
        head_.value.store(current_head + 1, std::memory_order_release);
        return true;
    }

    size_t size() const {
        return tail_.value.load(std::memory_order_acquire) - 
               head_.value.load(std::memory_order_acquire);
    }

    bool empty() const { return size() == 0; }
    bool full() const { return size() >= max_entries_; }
};

class ThreadLocalBatch {
private:
    static constexpr size_t BATCH_SIZE = 32;
    static constexpr size_t ENTRY_SIZE = 256;
    std::array<char, ENTRY_SIZE * BATCH_SIZE> batch_;
    size_t count_ = 0;
    const size_t entry_size_;

public:
    ThreadLocalBatch() : entry_size_(ENTRY_SIZE) {}
    
    bool try_add(const void* data, size_t len) {
        if (count_ >= BATCH_SIZE) return false;
        char* slot = &batch_[count_ * entry_size_];
        memcpy(slot, data, std::min(len, entry_size_ - 2));
        *reinterpret_cast<uint16_t*>(slot + entry_size_ - 2) = static_cast<uint16_t>(len);
        ++count_;
        return true;
    }

    const char* operator[](size_t idx) const { return &batch_[idx * entry_size_]; }
    size_t size() const { return count_; }
    void clear() { count_ = 0; }
};

template<typename Sink, LogLevel MinLevel = LogLevel::TRACE>
class Logger {
private:
    Sink sink_;
    std::unique_ptr<LockFreeRingBuffer> queue_;
    std::unique_ptr<std::thread> worker_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> running_{true};
    static constexpr size_t BACKOFF_MAX = 4;
    thread_local static inline fmt::memory_buffer format_buf_;
    thread_local static inline ThreadLocalBatch batch_;
    
    void worker_loop() {
        char entry[256];
        size_t len;
        while (running_.load(std::memory_order_acquire)) {
            if (queue_->try_dequeue(entry, len)) {
                sink_.write({entry, len});
            } else {
                std::unique_lock<std::mutex> lock(mtx_);
                cv_.wait_for(lock, std::chrono::microseconds(100));
            }
        }
        while (queue_->try_dequeue(entry, len)) {
            sink_.write({entry, len});
        }
    }

    void flush_batch() {
        for (size_t i = 0; i < batch_.size(); ++i) {
            const char* data = batch_[i];
            size_t len = *reinterpret_cast<const uint16_t*>(data + 254);
            int backoff = 0;
            while (!queue_->try_enqueue(data, len)) {
                if (backoff < BACKOFF_MAX) {
                    for (int j = 0; j < (1 << backoff); ++j) {
                        std::this_thread::yield();
                    }
                    ++backoff;
                } else {
                    std::this_thread::sleep_for(std::chrono::nanoseconds(100));
                }
            }
        }
        batch_.clear();
        if (batch_.size() > 0) {
            std::lock_guard<std::mutex> lock(mtx_);
            cv_.notify_one();
        }
    }

public:
    explicit Logger(Sink sink = {}, bool async = false) 
        : sink_(std::move(sink)) {
        if (async) {
            queue_ = std::make_unique<LockFreeRingBuffer>(256, 65536);
            worker_ = std::make_unique<std::thread>(&Logger::worker_loop, this);
        }
    }
    
    ~Logger() {
        if (worker_) {
            running_ = false;
            cv_.notify_all();
            worker_->join();
        }
        flush();
    }

    template<LogLevel L, typename... Args>
    void log(fmt::format_string<Args...> fmt, Args&&... args) {
        if constexpr (static_cast<int>(L) >= static_cast<int>(MinLevel)) {
            auto& buf = format_buf_;
            buf.clear();
            auto now = std::chrono::steady_clock::now().time_since_epoch();
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
            fmt::format_to(std::back_inserter(buf), "{}.{} ", ns / 1'000'000'000, ns % 1'000'000'000);
            constexpr const char levels[] = "TDIWEC";
            fmt::format_to(std::back_inserter(buf), "{} ", levels[static_cast<int>(L)]);
            fmt::format_to(std::back_inserter(buf), fmt, std::forward<Args>(args)...);
            buf.push_back('\n');
            if (queue_) {
                if (!batch_.try_add(buf.data(), buf.size())) {
                    flush_batch();
                    batch_.try_add(buf.data(), buf.size());
                }
            } else {
                sink_.write({buf.data(), buf.size()});
            }
        }
    }

    void flush() {
        if (batch_.size() > 0) {
            flush_batch();
        }
        if (worker_) {
            while (!queue_->empty()) {
                std::this_thread::yield();
            }
        }
        sink_.flush();
    }

    template<typename... Args> void trace(fmt::format_string<Args...> fmt, Args&&... args){log<LogLevel::TRACE>(fmt, std::forward<Args>(args)...);}
    template<typename... Args> void debug(fmt::format_string<Args...> fmt, Args&&... args){log<LogLevel::DEBUG>(fmt, std::forward<Args>(args)...);}
    template<typename... Args> void info(fmt::format_string<Args...> fmt, Args&&... args){log<LogLevel::INFO>(fmt, std::forward<Args>(args)...);}
    template<typename... Args> void warn(fmt::format_string<Args...> fmt, Args&&... args){log<LogLevel::WARN>(fmt, std::forward<Args>(args)...);}
    template<typename... Args> void error(fmt::format_string<Args...> fmt, Args&&... args){log<LogLevel::ERROR>(fmt, std::forward<Args>(args)...);}
    template<typename... Args> void critical(fmt::format_string<Args...> fmt, Args&&... args){log<LogLevel::CRITICAL>(fmt, std::forward<Args>(args)...);}
};

struct NullSink {
    void write(std::string_view) {}
    void flush() {}
};

struct StdoutSink {
    void write(std::string_view sv) { 
        fwrite(sv.data(), 1, sv.size(), stdout); 
    }
    void flush() { fflush(stdout); }
};

// ✅ FIX: Add factory function declarations
std::shared_ptr<Logger<StdoutSink>> stdout_logger_mt(const char* name);
std::shared_ptr<Logger<NullSink>> null_logger_mt(const char* name);

} // namespace zerolog
