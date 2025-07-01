# CMakeLists.txt for Order Book Tests

# Include FetchContent for Google Test
include(FetchContent)

# Fetch Google Test
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0  # Use a stable release
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Make Google Test available
FetchContent_MakeAvailable(googletest)

# Include GoogleTest
include(GoogleTest)

# Define test executable
add_executable(orderbook_tests
    test_main.cpp
    test_order_book.cpp
    test_price_level.cpp
    test_helpers.cpp
    test_integration.cpp
)

# Link test executable with libraries
target_link_libraries(orderbook_tests
    PRIVATE
        OrderBook::OrderBook
        gtest
        gtest_main
        gmock
        gmock_main
)

# Include directories for tests
target_include_directories(orderbook_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Set C++ standard for tests
target_compile_features(orderbook_tests PRIVATE cxx_std_17)

# Enable testing and discover tests
enable_testing()
gtest_discover_tests(orderbook_tests)

# Add compiler warnings for tests
target_compile_options(orderbook_tests PRIVATE
    $<$<CONFIG:Debug>:-g -O0 -Wall -Wextra -Wpedantic>
    $<$<CONFIG:Release>:-O2 -DNDEBUG>
)

# Create a custom target to run tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS orderbook_tests
    COMMENT "Running Order Book tests"
)
