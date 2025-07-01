#!/bin/bash

# Build and run the Databento + OrderBook integration example
# This script demonstrates FetchContent integration with the Databento C++ library

set -e

echo "=== Building Databento + OrderBook Integration Example ==="

# Create build directory
mkdir -p build
cd build

# Configure CMake (requires CMake 3.24+ for Databento)
echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build the project
echo "Building..."
make -j$(nproc)

echo "Build completed successfully!"
echo ""
echo "=== Running the demo ==="
echo "Note: Set DATABENTO_API_KEY environment variable for real data access"
echo ""

# Run the example
./consumer_fetchcontent

echo ""
echo "=== Demo completed ==="
echo ""
echo "To run with real Databento data:"
echo "  export DATABENTO_API_KEY=your_api_key_here"
echo "  ./consumer_fetchcontent"
echo ""
echo "Features demonstrated:"
echo "  - CMake FetchContent integration"
echo "  - Databento C++ library usage"
echo "  - MBO (Market By Order) data processing"
echo "  - ES S&P 500 futures data from CME Globex"
echo "  - Real-time order book management"
