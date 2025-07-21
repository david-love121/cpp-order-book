#!/bin/bash
set -e
export CXX="/usr/bin/clang++"
export CC="/usr/bin/clang"
# Create a build directory if it doesn't exist
mkdir -p build

# Navigate into the build directory
cd build

# Configure the project with CMake for Ninja
cmake ../ -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build the project with Ninja
ninja