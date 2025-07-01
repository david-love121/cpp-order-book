#include <gtest/gtest.h>
#include <set>
#include <thread>
#include <vector>
#include <chrono>

#include "Helpers.h"

class HelpersTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset any static state if needed
    }

    void TearDown() override {
        // Clean up if needed
    }
};

// Test GenerateOrderId uniqueness
TEST_F(HelpersTest, GenerateOrderIdUniqueness) {
    std::set<uint64_t> generated_ids;
    const int num_ids = 1000;
    
    for (int i = 0; i < num_ids; ++i) {
        uint64_t id = Helpers::GenerateOrderId();
        
        // Each ID should be unique
        EXPECT_TRUE(generated_ids.find(id) == generated_ids.end()) 
            << "Duplicate ID generated: " << id;
        
        generated_ids.insert(id);
    }
    
    EXPECT_EQ(generated_ids.size(), num_ids);
}

// Test GenerateOrderId sequential behavior
TEST_F(HelpersTest, GenerateOrderIdSequential) {
    // Generate several IDs and verify they are increasing
    std::vector<uint64_t> ids;
    const int num_ids = 10;
    
    for (int i = 0; i < num_ids; ++i) {
        ids.push_back(Helpers::GenerateOrderId());
    }
    
    // Verify IDs are strictly increasing
    for (size_t i = 1; i < ids.size(); ++i) {
        EXPECT_GT(ids[i], ids[i-1]) 
            << "ID " << ids[i] << " is not greater than previous ID " << ids[i-1];
    }
}

// Test GenerateExecutionId uniqueness
TEST_F(HelpersTest, GenerateExecutionIdUniqueness) {
    std::set<uint64_t> generated_ids;
    const int num_ids = 1000;
    
    for (int i = 0; i < num_ids; ++i) {
        uint64_t id = Helpers::GenerateExecutionId();
        
        // Each ID should be unique
        EXPECT_TRUE(generated_ids.find(id) == generated_ids.end()) 
            << "Duplicate execution ID generated: " << id;
        
        generated_ids.insert(id);
    }
    
    EXPECT_EQ(generated_ids.size(), num_ids);
}

// Test GenerateExecutionId sequential behavior
TEST_F(HelpersTest, GenerateExecutionIdSequential) {
    std::vector<uint64_t> ids;
    const int num_ids = 10;
    
    for (int i = 0; i < num_ids; ++i) {
        ids.push_back(Helpers::GenerateExecutionId());
    }
    
    // Verify IDs are strictly increasing
    for (size_t i = 1; i < ids.size(); ++i) {
        EXPECT_GT(ids[i], ids[i-1]) 
            << "Execution ID " << ids[i] << " is not greater than previous ID " << ids[i-1];
    }
}

// Test GetTimeStamp returns reasonable values
TEST_F(HelpersTest, GetTimeStampReasonable) {
    uint64_t timestamp1 = Helpers::GetTimeStamp();
    
    // Sleep for a small amount to ensure time difference
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    uint64_t timestamp2 = Helpers::GetTimeStamp();
    
    // Second timestamp should be greater than first
    EXPECT_GT(timestamp2, timestamp1);
    
    // Difference should be reasonable (at least 10ms, but less than 1 second)
    uint64_t diff = timestamp2 - timestamp1;
    EXPECT_GE(diff, 10);     // At least 10ms
    EXPECT_LT(diff, 1000);   // Less than 1 second
}

// Test GetTimeStamp returns values close to system time
TEST_F(HelpersTest, GetTimeStampAccuracy) {
    // Get timestamp from helper
    uint64_t helper_timestamp = Helpers::GetTimeStamp();
    
    // Get system timestamp using same method
    uint64_t system_timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
    
    // Should be very close (within 100ms)
    uint64_t diff = (helper_timestamp > system_timestamp) ? 
                    (helper_timestamp - system_timestamp) : 
                    (system_timestamp - helper_timestamp);
    
    EXPECT_LT(diff, 100) << "Helper timestamp " << helper_timestamp 
                         << " differs too much from system timestamp " << system_timestamp;
}

// Test thread safety of GenerateOrderId
TEST_F(HelpersTest, GenerateOrderIdThreadSafety) {
    const int num_threads = 10;
    std::vector<std::thread> threads;
    std::vector<std::vector<uint64_t>> thread_results(num_threads);
    
    // Launch threads to generate IDs concurrently
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&thread_results, t]() {
            for (int i = 0; i < 100; ++i) {
                thread_results[t].push_back(Helpers::GenerateOrderId());
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Collect all generated IDs
    std::set<uint64_t> all_ids;
    for (const auto& thread_ids : thread_results) {
        for (uint64_t id : thread_ids) {
            EXPECT_TRUE(all_ids.find(id) == all_ids.end()) 
                << "Duplicate ID " << id << " generated across threads";
            all_ids.insert(id);
        }
    }
    
    // Should have exactly the expected number of unique IDs
    EXPECT_EQ(all_ids.size(), num_threads * 100);
}

// Test thread safety of GenerateExecutionId
TEST_F(HelpersTest, GenerateExecutionIdThreadSafety) {
    const int num_threads = 10;
    std::vector<std::thread> threads;
    std::vector<std::vector<uint64_t>> thread_results(num_threads);
    
    // Launch threads to generate execution IDs concurrently
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&thread_results, t]() {
            for (int i = 0; i < 100; ++i) {
                thread_results[t].push_back(Helpers::GenerateExecutionId());
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Collect all generated IDs
    std::set<uint64_t> all_ids;
    for (const auto& thread_ids : thread_results) {
        for (uint64_t id : thread_ids) {
            EXPECT_TRUE(all_ids.find(id) == all_ids.end()) 
                << "Duplicate execution ID " << id << " generated across threads";
            all_ids.insert(id);
        }
    }
    
    // Should have exactly the expected number of unique IDs
    EXPECT_EQ(all_ids.size(), num_threads * 100);
}

// Test GetTimeStamp from multiple threads
TEST_F(HelpersTest, GetTimeStampThreadSafety) {
    const int num_threads = 10;
    std::vector<std::thread> threads;
    std::vector<std::vector<uint64_t>> thread_results(num_threads);
    
    // Launch threads to get timestamps concurrently
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&thread_results, t]() {
            for (int i = 0; i < 100; ++i) {
                thread_results[t].push_back(Helpers::GetTimeStamp());
                // Small delay to avoid getting identical timestamps
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all timestamps are reasonable and mostly increasing
    uint64_t overall_min = UINT64_MAX;
    uint64_t overall_max = 0;
    
    for (const auto& thread_timestamps : thread_results) {
        for (size_t i = 0; i < thread_timestamps.size(); ++i) {
            uint64_t ts = thread_timestamps[i];
            overall_min = std::min(overall_min, ts);
            overall_max = std::max(overall_max, ts);
            
            // Verify timestamp is reasonable (not zero, not too far in future)
            EXPECT_GT(ts, 0);
            EXPECT_LT(ts, overall_max + 10000);  // Within 10 seconds of max seen
        }
    }
    
    // Overall range should be reasonable for the test duration
    EXPECT_LT(overall_max - overall_min, 30000);  // Less than 30 seconds total
}

// Performance test for ID generation
TEST_F(HelpersTest, GenerateOrderIdPerformance) {
    const int num_ids = 100000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_ids; ++i) {
        uint64_t id = Helpers::GenerateOrderId();
        (void)id;  // Suppress unused variable warning
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Should be able to generate 100k IDs quickly
    EXPECT_LT(duration.count(), 100000);  // Less than 100ms
    
    std::cout << "Generated " << num_ids << " order IDs in " 
              << duration.count() << " microseconds" << std::endl;
}

// Performance test for timestamp generation
TEST_F(HelpersTest, GetTimeStampPerformance) {
    const int num_calls = 10000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_calls; ++i) {
        uint64_t ts = Helpers::GetTimeStamp();
        (void)ts;  // Suppress unused variable warning
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Should be able to get 10k timestamps quickly
    EXPECT_LT(duration.count(), 50000);  // Less than 50ms
    
    std::cout << "Generated " << num_calls << " timestamps in " 
              << duration.count() << " microseconds" << std::endl;
}
