#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Main function for running tests
// Google Test will provide its own main, but we can customize it if needed
int main(int argc, char **argv) {
    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);
    
    // Optionally, initialize Google Mock (if using mocks)
    ::testing::InitGoogleMock(&argc, argv);
    
    // Run all tests
    return RUN_ALL_TESTS();
}
