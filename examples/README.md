# Order Book Library Usage Examples

This directory contains examples demonstrating how to use the C++ Order Book library in other CMake projects.

## Available Examples

### 1. FetchContent Consumer (`cpp_consumer_fetchcontent/`)

**✅ WORKING** - Demonstrates using CMake's FetchContent to include the order book library directly in your project.

**Features:**
- Automatic source download/inclusion
- No installation required
- Perfect for development and debugging
- Cross-platform compatible

**How to use:**
```bash
cd cpp_consumer_fetchcontent
mkdir build && cd build
cmake ..
make
./consumer_fetchcontent
```

**When to use:**
- Development and testing
- When you want to include source code directly
- When you need to debug library code
- For projects that don't want system dependencies

### 2. Installed Consumer (`cpp_consumer_installed/`)

**🚧 TODO** - Will demonstrate using `find_package()` with a system-installed version of the library.

**When to use:**
- Production deployments
- When library is installed system-wide
- For cleaner dependency management
- When multiple projects use the same library version

## Quick Start with FetchContent

1. **Create your project structure:**
```
your_project/
├── CMakeLists.txt
├── main.cpp
└── build/
```

2. **CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.15)
project(MyApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    OrderBook
    GIT_REPOSITORY https://github.com/yourusername/cpp_order_book.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(OrderBook)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE OrderBook::OrderBook)
```

3. **main.cpp:**
```cpp
#include "OrderBook.h"
#include <iostream>

int main() {
    OrderBook book;
    book.AddOrder(1, 1, false, 100, 10050); // Sell 100 @ 100.50
    book.AddOrder(2, 2, true, 80, 10040);   // Buy 80 @ 100.40
    
    std::cout << "Best Bid: " << book.GetBestBid() << std::endl;
    std::cout << "Best Ask: " << book.GetBestAsk() << std::endl;
    return 0;
}
```

4. **Build and run:**
```bash
mkdir build && cd build
cmake ..
make
./my_app
```

## Library API

The order book library provides a simple but powerful API:

```cpp
class OrderBook {
public:
    // Order management
    void AddOrder(uint64_t order_id, uint64_t user_id, bool is_buy, uint64_t quantity, uint64_t price);
    void CancelOrder(uint64_t order_id);
    void ModifyOrder(uint64_t order_id, uint64_t new_quantity, uint64_t new_price);

    // Market data
    uint64_t GetBestBid() const;
    uint64_t GetBestAsk() const;
    uint64_t GetTotalBidVolume() const;
    uint64_t GetTotalAskVolume() const;
};
```

## Build Requirements

- CMake 3.15+
- C++17 compatible compiler
- No external dependencies (self-contained)

## Integration Methods Comparison

| Method | Installation | Build Time | Debugging | Use Case |
|--------|-------------|------------|-----------|----------|
| FetchContent | None | Longer | Easy | Development, Testing |
| find_package | Required | Faster | Limited | Production, Distribution |
| ExternalProject | None | Longest | Medium | Complex builds |

**Recommendation:** Start with FetchContent for development, switch to find_package for production.
