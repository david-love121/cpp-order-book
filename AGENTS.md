# Agent Instructions for C++ Order Book


## Code Style Guidelines
- **C++ Standard**: C++17 required
- **Include style**: Local headers in quotes, system headers in angle brackets
- **Naming**: snake_case for variables/functions, PascalCase for classes
- **Member variables**: Trailing underscore (e.g., `order_map_`)
- **Types**: Use `uint64_t` for IDs, quantities, prices, timestamps
- **Memory**: Manual new/delete for Order objects, RAII for everything else
- **Error handling**: Throw exceptions for invalid inputs, notify clients for rejections
- **Comments**: Minimal comments, prefer self-documenting code
- **Headers**: Use `#pragma once`, forward declarations where possible
- **Const correctness**: Mark methods const when they don't modify state