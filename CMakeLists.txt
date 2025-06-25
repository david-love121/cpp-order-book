# Minimum CMake version required
cmake_minimum_required(VERSION 3.15)

# Define your project name
project(OrderBookLibrary LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# Add subdirectories
add_subdirectory(include)
add_subdirectory(src)