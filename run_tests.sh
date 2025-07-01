#!/bin/bash

cd /home/david/repos/cpp_order_book/build

echo "Building tests..."
make -j$(nproc) > /dev/null 2>&1

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo "Running all Order Book tests..."
echo "==============================="

./tests/orderbook_tests --gtest_color=yes
