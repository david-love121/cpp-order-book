#!/bin/bash

echo "🐛 Testing VS Code Debugging Setup from Root Workspace"
echo "======================================================"
echo

# Check that the executable exists and has debug symbols
EXECUTABLE="/home/david/repos/cpp_order_book/examples/cpp_consumer_fetchcontent/build/consumer_fetchcontent"

if [ -f "$EXECUTABLE" ]; then
    echo "✅ Executable found: $EXECUTABLE"
    
    # Check for debug symbols
    if objdump -h "$EXECUTABLE" | grep -q debug; then
        echo "✅ Debug symbols found in executable"
    else
        echo "⚠️  Debug symbols might be missing"
    fi
    
    # Check file size (debug builds should be larger)
    SIZE=$(du -h "$EXECUTABLE" | cut -f1)
    echo "📊 Executable size: $SIZE (good for debug build)"
else
    echo "❌ Executable not found!"
    exit 1
fi

echo
echo "🎯 VS Code Debug Configurations Available:"
echo "1. 'Debug MBO Integration Example' - Full Databento API with breakpoints"
echo "2. 'Debug Basic OrderBook Demo Only' - No API key needed"
echo "3. 'Debug Main OrderBook Project' - Core library debugging"
echo "4. 'Debug MBO Tests' - Unit test debugging"

echo
echo "🚀 Quick Test Instructions:"
echo "1. Open main.cpp in VS Code"
echo "2. Set breakpoint on line ~62: uint64_t price = static_cast<uint64_t>(mbo->price);"
echo "3. Press F5, select 'Debug Basic OrderBook Demo Only'"
echo "4. Should hit breakpoints in the basic demo first"

echo
echo "🔧 For Price Debugging:"
echo "1. Set DATABENTO_API_KEY: export DATABENTO_API_KEY=your_key"
echo "2. Press F5, select 'Debug MBO Integration Example'"
echo "3. When stopped at price breakpoint, inspect variables in Debug Console"

echo
echo "Ready to debug! 🎯"
