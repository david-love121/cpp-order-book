cmake_minimum_required(VERSION 3.15)

project(OrderBookConsumerInstalled LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find the OrderBook library
# This assumes the library has been installed via 'cmake --install'
find_package(OrderBook REQUIRED)

# Create the executable
add_executable(consumer_installed main.cpp)

# Link with the OrderBook library
target_link_libraries(consumer_installed 
    PRIVATE 
        OrderBook::OrderBook
)

# Set output directory for cleaner builds
set_target_properties(consumer_installed
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
