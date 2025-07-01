# Debugging the MBO Integration with VS Code and Built-in C++ Tools

## 🛠️ **Setup Complete**

Your VS Code debugging environment is now configured using the **built-in C++ extension** with LLDB.

## 🎯 **Available Debug Configurations**

### 1. **Debug MBO Integration** 
- Debugs the full application with real Databento API
- Requires `DATABENTO_API_KEY` environment variable
- Best for debugging price scaling and MBO message processing

### 2. **Debug Basic OrderBook Demo Only**
- Runs without external API (no DATABENTO_API_KEY needed)
- Good for debugging order book logic without network dependencies
- Uses synthetic data only

### 3. **Debug MBO Integration Tests**
- Debugs the unit tests
- Best for testing individual components

### 4. **Debug - Stop at Price Conversion**
- Pre-configured for debugging the price scaling issue
- Focuses on the `/100.0` division problem

## 🐛 **Quick Start: Debug the Price Scaling Issue**

### **Step 1: Set Your API Key**
```bash
export DATABENTO_API_KEY=your_key_here
```

### **Step 2: Set Breakpoints**

1. **Open `main.cpp`**
2. **Click in the left margin** to set breakpoints at:

   - **Line ~62**: `uint64_t price = static_cast<uint64_t>(mbo->price);`
   - **Line ~68**: `<< " " << size << "@" << (price / 100.0) << std::endl;`
   - **Line ~232**: `std::cout << "Best Bid: " << (best_bid / 100.0) << std::endl;`

### **Step 3: Start Debugging**

1. **Press `F5`** or **Run → Start Debugging**
2. **Select "Debug MBO Integration"**
3. **Wait for the first MBO message** to hit your breakpoint

### **Step 4: Inspect the Price Values**

When the debugger stops:

1. **Hover over `mbo->price`** to see the raw value (~59,485,000,000)
2. **In the Debug Console**, type:
   ```
   mbo->price
   price
   (double)price / 100.0
   (double)price / 10000000.0
   ```
3. **Compare the results** to find the correct scaling factor

## 🔍 **Debugging Commands**

### **In the Debug Console** (when paused):

```cpp
// Print raw Databento price
mbo->price

// Print current conversion
(double)price / 100.0

// Test different scaling factors
(double)price / 1000000.0
(double)price / 10000000.0

// Check symbol name
symbol

// Check MBO action type
(int)mbo->action

// Print order book state
order_book_.GetBestBid()
order_book_.GetBestAsk()
```

## 🎮 **Debugging Workflow**

### **To Fix the Price Issue:**

1. **Set breakpoint** on the price conversion line
2. **Start "Debug MBO Integration"**
3. **When it hits the breakpoint:**
   - Note the raw `mbo->price` value
   - Try different divisors in Debug Console
   - Find the one that gives ~5948.5 for ES futures
4. **Modify the code** with the correct scaling factor
5. **Test again**

### **VS Code Debugging Shortcuts:**

- **F5**: Start debugging / Continue
- **F9**: Toggle breakpoint
- **F10**: Step over (next line)
- **F11**: Step into (enter function)
- **Shift+F11**: Step out (exit function)
- **Ctrl+Shift+F5**: Restart debugging
- **Shift+F5**: Stop debugging

## 🛠️ **Build and Run Tasks**

**Access via `Ctrl+Shift+P` → "Tasks: Run Task":**

- **`build-debug`** - Build in debug mode with symbols
- **`configure-debug`** - Configure CMake for debugging
- **`run-basic-demo`** - Run without API key (basic demo only)
- **`run-with-api-key`** - Run with real data

## 🔧 **Debug Without Network Dependencies**

To debug just the order book logic:

1. **Select "Debug Basic OrderBook Demo Only"**
2. **This skips the Databento API calls**
3. **Uses synthetic prices like 4150.25** (these work correctly)
4. **Good for testing order book mechanics**

## 💡 **Expected Debugging Results**

Based on your previous output:

- **Raw `mbo->price`**: ~59,485,000,000
- **Current `/100.0`**: ~594,850,000 ❌ (way too big)
- **Try `/10,000,000.0`**: ~5.9485 ✅ (looks right for ES futures)

The issue is likely that Databento uses a different price scaling than the assumed centipenny (1/100) format.

## 🚀 **Quick Test**

1. **F5** to start debugging
2. **Let it hit the first MBO breakpoint**
3. **In Debug Console, type:** `(double)price / 10000000.0`
4. **See if you get a realistic ES futures price** (~5948.5)

That's it! The built-in VS Code C++ debugger should work reliably for inspecting the price scaling issue. 🎯
