# CMakeLists.txt for OrderBook Library Headers

# Collect all header files
set(ORDERBOOK_HEADERS
    Interfaces.h
    OrderBook.h
    PriceLevel.h
    Types.h
    Helpers.h
)

# Create an interface library for headers
add_library(OrderBookHeaders INTERFACE)

# Set the include directories for the library
target_include_directories(OrderBookHeaders
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Set C++ standard for the library
target_compile_features(OrderBookHeaders INTERFACE cxx_std_17)

# Install headers for external use
install(FILES ${ORDERBOOK_HEADERS}
    DESTINATION include/orderbook
)
