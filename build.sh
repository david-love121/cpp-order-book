#!/bin/bash

# C++ Order Book - Automated Build Script
# This script will clean, configure, and build the project

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Get script directory (project root)
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

print_status "Starting C++ Order Book build process..."
print_status "Project root: $PROJECT_ROOT"
print_status "Build directory: $BUILD_DIR"

# Parse command line arguments
BUILD_TYPE="Debug"
CLEAN_BUILD=false
VERBOSE=false
RUN_TESTS=false
INSTALL_LIB=false
SANITIZERS=false
JOBS=$(nproc)

while [[ $# -gt 0 ]]; do
    case $1 in
        -r|--release)
            BUILD_TYPE="Release"
            shift
            ;;
        -rd|--relwithdebinfo)
            BUILD_TYPE="RelWithDebInfo"
            shift
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -t|--test)
            RUN_TESTS=true
            shift
            ;;
        -i|--install)
            INSTALL_LIB=true
            shift
            ;;
        -s|--sanitizers)
            SANITIZERS=true
            shift
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  -r, --release           Build in Release mode (optimized)"
            echo "  -rd, --relwithdebinfo   Build in RelWithDebInfo mode"
            echo "  -c, --clean             Clean build (remove build directory)"
            echo "  -v, --verbose           Verbose build output"
            echo "  -t, --test              Run tests after building"
            echo "  -i, --install           Install library after building"
            echo "  -s, --sanitizers        Enable AddressSanitizer and UBSan"
            echo "  -j, --jobs NUM          Number of parallel jobs (default: $(nproc))"
            echo "  -h, --help              Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                      # Debug build"
            echo "  $0 --release            # Release build"
            echo "  $0 --clean --release    # Clean release build"
            echo "  $0 --test               # Build and run tests"
            echo "  $0 --sanitizers --test  # Build with sanitizers and test"
            echo "  $0 -j 4                 # Use 4 parallel jobs"
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

print_status "Build configuration:"
print_status "  Build type: $BUILD_TYPE"
print_status "  Clean build: $CLEAN_BUILD"
print_status "  Run tests: $RUN_TESTS"
print_status "  Install library: $INSTALL_LIB"
print_status "  Sanitizers: $SANITIZERS"
print_status "  Parallel jobs: $JOBS"
print_status "  Verbose: $VERBOSE"

# Navigate to project root
cd "$PROJECT_ROOT"

# Clean build if requested
if [ "$CLEAN_BUILD" = true ]; then
    print_status "Cleaning previous build..."
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        print_success "Build directory cleaned"
    else
        print_warning "No existing build directory to clean"
    fi
fi

# Create build directory
print_status "Creating build directory..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
print_status "Configuring project with CMake..."
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=$BUILD_TYPE"

if [ "$VERBOSE" = true ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_VERBOSE_MAKEFILE=ON"
fi

# Add sanitizers if requested
if [ "$SANITIZERS" = true ]; then
    print_status "Enabling AddressSanitizer and UBSan..."
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -DCMAKE_C_FLAGS=-fsanitize=address,undefined"
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined"
    export ASAN_OPTIONS="detect_leaks=1:abort_on_error=1"
    export UBSAN_OPTIONS="print_stacktrace=1:abort_on_error=1"
fi

# Check for required tools
if ! command -v cmake &> /dev/null; then
    print_error "CMake is not installed or not in PATH"
    exit 1
fi

if ! command -v make &> /dev/null; then
    print_error "Make is not installed or not in PATH"
    exit 1
fi

if ! cmake $CMAKE_ARGS ..; then
    print_error "CMake configuration failed!"
    exit 1
fi

print_success "CMake configuration completed"

# Build the project
print_status "Building project with $JOBS parallel jobs..."
BUILD_START_TIME=$(date +%s)

MAKE_ARGS=""
if [ "$VERBOSE" = true ]; then
    MAKE_ARGS="VERBOSE=1"
fi

if ! make -j"$JOBS" $MAKE_ARGS; then
    print_error "Build failed!"
    exit 1
fi

BUILD_END_TIME=$(date +%s)
BUILD_DURATION=$((BUILD_END_TIME - BUILD_START_TIME))

print_success "Build completed successfully!"
print_success "Build time: ${BUILD_DURATION} seconds"

# Run tests if requested
if [ "$RUN_TESTS" = true ]; then
    print_status "Running tests..."
    TEST_START_TIME=$(date +%s)
    
    # Check if test executable exists
    if [ -f "$BUILD_DIR/tests/orderbook_tests" ]; then
        print_status "Running C++ Order Book test suite..."
        if "$BUILD_DIR/tests/orderbook_tests" --gtest_color=yes; then
            TEST_END_TIME=$(date +%s)
            TEST_DURATION=$((TEST_END_TIME - TEST_START_TIME))
            print_success "All tests passed! Test time: ${TEST_DURATION} seconds"
        else
            print_error "Some tests failed!"
            exit 1
        fi
    else
        print_warning "Test executable not found. Skipping tests."
    fi
fi

# Install library if requested
if [ "$INSTALL_LIB" = true ]; then
    print_status "Installing library..."
    if make install; then
        print_success "Library installed successfully!"
    else
        print_error "Installation failed!"
        exit 1
    fi
fi

# Show build artifacts
print_status "Build artifacts in $BUILD_DIR:"
find "$BUILD_DIR" -name "*.a" -o -name "*.so" -o -name "libOrderBookLib*" -o -name "*tests*" -type f 2>/dev/null | while read -r file; do
    if [ -f "$file" ]; then
        size=$(du -h "$file" | cut -f1)
        echo "  $(basename "$file") ($size)"
    fi
done

# Display useful information
print_status ""
print_status "Next steps:"
if [ "$RUN_TESTS" = false ]; then
    print_status "  Run tests: ./build/tests/orderbook_tests"
    print_status "  Or use:    ./run_tests.sh"
fi
if [ "$INSTALL_LIB" = false ]; then
    print_status "  Install:   make install (from build directory)"
fi
print_status "  Examples:  Check examples/ directory for usage"
print_status "  Docs:      See README.md for documentation"
print_status ""

print_success "All done! Your C++ Order Book is ready."
