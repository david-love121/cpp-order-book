cmake_minimum_required(VERSION 3.24)  # Databento requires 3.24+

project(OrderBookConsumerFetchContent LANGUAGES CXX)

# Set C++ standard (Databento requires C++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include FetchContent module
include(FetchContent)

# Fetch the OrderBook library from the parent directory
# In a real project, you would use a Git repository URL
FetchContent_Declare(
    OrderBook
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

# Fetch Databento C++ library
FetchContent_Declare(
    databento
    GIT_REPOSITORY https://github.com/databento/databento-cpp
    GIT_TAG HEAD
)

# Make both libraries available
FetchContent_MakeAvailable(OrderBook databento)

# Create the consumer executable
add_executable(consumer_fetchcontent 
    main.cpp
    DatabentoMboClient.cpp
    OrderBookManager.cpp
    PortfolioManager.cpp
    Strategy.cpp
    TopOfBookTracker.cpp
)

# Link with both OrderBook and Databento libraries
target_link_libraries(consumer_fetchcontent 
    PRIVATE 
        OrderBook::OrderBook
        databento::databento
)

# Create test executable for MBO integration

# Enable testing
enable_testing()


# Optional: Set some compile flags for better debugging/optimization
target_compile_options(consumer_fetchcontent PRIVATE
    $<$<CONFIG:Debug>:-g -O0 -Wall -Wextra>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)