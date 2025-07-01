#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <vector>

class Helpers {
 public:
    static uint64_t GenerateOrderId() {
        static std::atomic<uint64_t> id_counter{0};
        return id_counter.fetch_add(1, std::memory_order_relaxed);
    }
    static uint64_t GenerateExecutionId() {
        return last_order_number.fetch_add(1, std::memory_order_relaxed);
    }
    static uint64_t GetTimeStamp() {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }
    private:
    static std::atomic<uint64_t> last_order_number;

};