# Minimum CMake version required
cmake_minimum_required(VERSION 3.15)

# Define your project name
project(OrderBookLibrary LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# FetchContent for external dependencies
include(FetchContent)
FetchContent_Declare(
  databento
  GIT_REPOSITORY https://github.com/databento/databento-cpp
  GIT_TAG HEAD
)
FetchContent_MakeAvailable(databento)

# Add subdirectories
add_subdirectory(include)
add_subdirectory(src)