# CMakeLists.txt for OrderBook Library Source Files

# Collect all source files
set(ORDERBOOK_SOURCES
    OrderBook.cpp
    PriceLevel.cpp
    Helpers.cpp
)

# Create the OrderBook library
add_library(OrderBookLib STATIC
    ${ORDERBOOK_SOURCES}
)

# Set the include directories for the library
target_include_directories(OrderBookLib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include/orderbook>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Set C++ standard for the library
target_compile_features(OrderBookLib PUBLIC cxx_std_17)

# Link with headers and dependencies
target_link_libraries(OrderBookLib
    PUBLIC
        OrderBookHeaders
)

# Install the library
install(TARGETS OrderBookLib OrderBookHeaders
    EXPORT OrderBookTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Create an alias for easier linking
add_library(OrderBook::OrderBook ALIAS OrderBookLib)
