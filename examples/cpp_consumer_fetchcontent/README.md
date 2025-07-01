# OrderBook + Databento MBO Integration Example

This example demonstrates how to integrate the C++ OrderBook library with Databento's market data API using CMake FetchContent, focusing on Market By Order (MBO) data for ES S&P 500 futures.

## Features

- **CMake FetchContent Integration**: Automatically fetches and builds both OrderBook and Databento libraries
- **MBO Data Processing**: Uses Market By Order (MBO) data for full order book depth
- **ES S&P 500 Futures**: Focuses on ES futures from CME Globex (GLBX.MDP3)
- **Real-time Order Book**: Maintains a live order book state from market data
- **Historical Data**: Processes historical MBO data for backtesting and analysis
- **FetchContent Integration**: Shows how to integrate multiple C++ libraries seamlessly

## Requirements

### System Dependencies

The Databento C++ library requires certain system dependencies:

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install libssl-dev libzstd-dev
```

**macOS:**
```bash
brew install openssl@3 zstd
```

### CMake Requirements

- CMake 3.24 or higher (required by Databento)
- C++17 compiler

### API Key

To use live or historical market data, you need a Databento API key:

1. Sign up at [databento.com](https://databento.com)
2. Get your API key from the dashboard
3. Set the environment variable:
   ```bash
   export DATABENTO_API_KEY="your-api-key-here"
   ```

## Building and Running

### Build the Example

From the `examples/cpp_consumer_fetchcontent` directory:

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

### Run the Example

```bash
# Run with API key for full functionality
export DATABENTO_API_KEY="your-api-key-here"
./consumer_fetchcontent

# Or run without API key (basic demo only)
./consumer_fetchcontent
```

## What the Example Does

### 1. Basic Order Book Demo
- Always runs, regardless of API key availability
- Demonstrates basic order book functionality
- Shows order matching and price-time priority

### 2. Live Market Data Demo (requires API key)
- Connects to Databento's live market data feed
- Subscribes to ES futures trades and quotes
- Generates synthetic orders based on market activity
- Runs for 30 seconds showing real-time order book updates

### 3. Historical Data Demo (requires API key)
- Fetches historical trade data from Databento
- Replays the data through the order book
- Demonstrates how to process batch market data

## Code Structure

### OrderBookManager Class
- Manages the integration between Databento and OrderBook
- Handles market data callbacks
- Generates synthetic order flow
- Provides order book status reporting

### Market Data Processing
- **Trade Messages**: Generate bid/ask orders around trade prices
- **Quote Messages**: Add orders based on market bid/ask spreads
- **Symbol Mapping**: Handle instrument ID to symbol conversion

### Synthetic Order Generation
- Creates realistic market making activity
- Adds orders with configurable spreads
- Simulates different trader behaviors

## Data Schema: Market By Order (MBO)

This example uses **MBO (Market By Order)** data which provides:
- Individual order additions, modifications, and cancellations  
- Full market depth with order-level granularity
- Real-time order book reconstruction
- Complete audit trail of market activity

### MBO vs Other Schemas
- **MBO**: Full order-level data (what this example uses)
- **MBP**: Market by price (aggregated levels)
- **Trades**: Trade executions only

## Market Data Details

- **Dataset**: GLBX.MDP3 (CME Globex)
- **Symbol**: ES.FUT (E-mini S&P 500 futures)
- **Schema**: MBO (Market By Order)
- **Time Range**: Configurable (default: recent 2-minute window)

### ES S&P 500 Futures
- **Contract**: E-mini S&P 500 futures (ES)
- **Exchange**: CME Globex
- **Tick Size**: 0.25 points ($12.50 per tick)
- **Trading Hours**: Nearly 24/5 (Sunday 5 PM - Friday 4 PM CT)

## Example Output

```
OrderBook + Databento Integration Demo
=====================================

=== Basic Order Book Demo (No External Data) ===
1. Adding initial orders...

=== Order Book Status ===
Best Bid: 4150.10
Best Ask: 4150.25
Total Bid Volume: 290
Total Ask Volume: 450

=== Live Market Data Demo ===
Starting live data stream for ES futures...

[MARKET DATA] Trade for ES.FUT - Price: 4150.50, Size: 10
[SYNTHETIC] Added bid 1001 (150@4150.40) and ask 1002 (120@4150.60)

=== Order Book Status ===
Best Bid: 4150.40
Best Ask: 4150.25
Total Bid Volume: 440
Total Ask Volume: 570
```

## Customization

### Market Making Parameters
```cpp
static constexpr uint64_t SPREAD_TICKS = 10;  // Adjust spread
static constexpr uint64_t ORDER_SIZE = 100;   // Adjust order sizes
```

### Symbols and Datasets
```cpp
// Change symbols
client.Subscribe({"NQ.FUT", "ES.FUT"}, Schema::Trades, SType::Parent);

// Change dataset
.SetDataset(Dataset::GlbxMdp3)  // CME Globex
```

### Time Ranges
```cpp
// Adjust historical data range
{"2024-06-28T14:30", "2024-06-28T14:35"}
```

## Integration Benefits

1. **Real Market Context**: Test order book logic with actual market conditions
2. **Realistic Data**: Use professional-grade market data for development
3. **Scalable Architecture**: Easy to extend for multiple instruments
4. **Production-Ready**: Foundation for real trading applications

## Next Steps

- Implement more sophisticated market making strategies
- Add risk management and position tracking
- Integrate with order management systems
- Add market data recording and replay functionality
- Implement advanced order types (iceberg, stop orders, etc.)

## References

- [Databento Documentation](https://databento.com/docs)
- [Databento C++ API](https://github.com/databento/databento-cpp)
- [CME Market Data](https://www.cmegroup.com/market-data.html)
