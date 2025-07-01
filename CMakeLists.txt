# Minimum CMake version required
cmake_minimum_required(VERSION 3.15)

# Define your project name
project(OrderBookLibrary LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include GNUInstallDirs for standard installation paths
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Option to build tests
option(BUILD_TESTS "Build unit tests" ON)

# Add subdirectories
add_subdirectory(include)
add_subdirectory(src)

# Add tests if enabled
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Generate and install package config files
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/OrderBookConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/OrderBookConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OrderBook
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/OrderBookConfigVersion.cmake"
    VERSION 1.0.0
    COMPATIBILITY SameMajorVersion
)

# Install config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/OrderBookConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/OrderBookConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OrderBook
)

# Export targets
install(EXPORT OrderBookTargets
    FILE OrderBookTargets.cmake
    NAMESPACE OrderBook::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OrderBook
)

# Demo executable for ModifyOrder functionality
add_executable(demo_modify_order demo_modify_order.cpp)
target_link_libraries(demo_modify_order PRIVATE OrderBookLib)