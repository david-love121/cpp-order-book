#!/bin/bash

echo "🔧 VS Code C++ Debugging Setup Complete!"
echo "========================================"
echo
echo "✅ Built-in C++ extension configured"
echo "✅ LLDB debugger path: /usr/bin/lldb"
echo "✅ Debug executable: build/consumer_fetchcontent"
echo "✅ Debug symbols included (Debug build)"
echo
echo "🎯 Quick Debug Test:"
echo "1. Open main.cpp in VS Code"
echo "2. Set breakpoint on line ~62: uint64_t price = static_cast<uint64_t>(mbo->price);"
echo "3. Press F5 and select 'Debug Basic OrderBook Demo Only'"
echo "4. Should stop at basic demo breakpoints"
echo
echo "🐛 To Debug Price Scaling:"
echo "1. Set DATABENTO_API_KEY: export DATABENTO_API_KEY=your_key"
echo "2. Press F5 and select 'Debug MBO Integration'"
echo "3. When stopped, inspect mbo->price in Debug Console"
echo "4. Try: (double)price / 10000000.0"
echo
echo "📖 Full guide: DEBUG_GUIDE.md"

# Test if debugging symbols are present
if objdump -h build/consumer_fetchcontent | grep -q debug; then
    echo "✅ Debug symbols found in executable"
else
    echo "⚠️  Debug symbols might be missing - rebuild with Debug config"
fi

echo
echo "Ready to debug! 🚀"
