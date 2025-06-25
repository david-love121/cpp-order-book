# CMakeLists.txt for OrderBook Library Source Files

# Collect all source files
set(ORDERBOOK_SOURCES
    OrderBook.cpp
    Commands.cpp
    Strategies.cpp
    PriceLevel.cpp
)

# Create the OrderBook library
add_library(OrderBookLib STATIC
    ${ORDERBOOK_SOURCES}
)

# Set the include directories for the library
target_include_directories(OrderBookLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Set C++ standard for the library
target_compile_features(OrderBookLib PUBLIC cxx_std_17)

# Link with headers and dependencies
target_link_libraries(OrderBookLib
    PUBLIC
        OrderBookHeaders
        databento::databento
)

# Install the library
install(TARGETS OrderBookLib
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# Create an alias for easier linking
add_library(OrderBook::OrderBook ALIAS OrderBookLib)
