# Minimum CMake version required
cmake_minimum_required(VERSION 3.24)

# Define your project name
project(OrderBookTrader LANGUAGES CXX)

option(BUILD_TESTING "Build the tests" ON)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# FetchContent for dependencies
include(FetchContent)
FetchContent_Declare(
    databento
    GIT_REPOSITORY https://github.com/databento/databento-cpp.git
    GIT_TAG HEAD
)
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # Add this for PIC
FetchContent_MakeAvailable(databento)

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.3.0
)
FetchContent_MakeAvailable(tomlplusplus)

FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v2.12.0
)
# pybind11 recommends this for best practices
set(PYBIND11_FINDPYTHON ON)
FetchContent_MakeAvailable(pybind11)


# Collect all source and header files
file(GLOB_RECURSE ORDERBOOK_SOURCES "src/*.cpp")
list(FILTER ORDERBOOK_SOURCES EXCLUDE REGEX ".*/(bindings|main)\\.cpp$") # Exclude bindings.cpp and main.cpp
file(GLOB_RECURSE ORDERBOOK_HEADERS "include/*.h")

# Create the OrderBook library
add_library(OrderBookLib STATIC
    ${ORDERBOOK_SOURCES}
    ${ORDERBOOK_HEADERS}
)
set_target_properties(OrderBookLib PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Set the include directories for the library
target_include_directories(OrderBookLib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link with headers and dependencies
target_link_libraries(OrderBookLib
    PUBLIC
        databento::databento
        tomlplusplus::tomlplusplus
)

# Add executable for the trader application
add_executable(orderbook_trader src/main.cpp)

# Link the trader executable with the OrderBook library
target_link_libraries(orderbook_trader
    PRIVATE
        OrderBookLib
)

# --- Testing ---
# The following block is included conditionally based on the BUILD_TESTING option.
# This is a standard practice to allow users of the library to include or exclude
# tests from their build.
if(BUILD_TESTING)
    # Enable testing for the project. This command includes the CTest module.
    enable_testing()

    # Fetch Google Test using FetchContent. This is the modern and recommended
    # way to handle dependencies in CMake. It downloads and configures the
    # dependency at configure-time.
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    # Define the test executable, listing all test source files.
    add_executable(orderbook_tests
        tests/test_main.cpp
        tests/test_order_book.cpp
        tests/test_price_level.cpp
        tests/test_integration.cpp
    )

    # Link the test executable against our library and Google Test.
    # Because OrderBookLib declares its include directories as PUBLIC,
    # orderbook_tests automatically gets the correct include paths.
    target_link_libraries(orderbook_tests
        PRIVATE
            OrderBookLib
            gmock_main
            gtest_main
    )

    # Include the GoogleTest module, which provides the gtest_discover_tests function.
    include(GoogleTest)

    # Automatically discover all tests in the executable. This is the standard
    # way to integrate Google Test with CTest, as it allows CTest to run
    # individual tests and provide detailed reports.
    gtest_discover_tests(orderbook_tests)
endif()

# --- Python Bindings ---
option(BUILD_PYTHON_BINDINGS "Build the python bindings" ON)

if(BUILD_PYTHON_BINDINGS)
    find_package(Python3 COMPONENTS Interpreter Development)
    if(NOT Python3_FOUND)
        find_package(Python REQUIRED COMPONENTS Interpreter Development)
    endif()
    
    pybind11_add_module(cpp_order_book_engine
        src/bindings.cpp
    )

    target_link_libraries(cpp_order_book_engine
        PRIVATE
            OrderBookLib
    )
endif()
