#include <iostream>
#include <vector>
#include <iomanip>
#include <chrono>
#include <thread>
#include <random>
#include <atomic>
#include <map>

// Include the OrderBook headers
#include "OrderBook.h"
#include "Trade.h"
#include "PriceLevel.h"
#include "Order.h"

// Include Databento headers
#include <databento/live.hpp>
#include <databento/historical.hpp>
#include <databento/symbol_map.hpp>
#include <databento/dbn.hpp>

// Include our cache implementation
#include "DatabentoCache.h"

using namespace databento;

class OrderBookManager {
private:
    OrderBook order_book_;
    PitSymbolMap symbol_mappings_;
    std::atomic<uint64_t> order_id_counter_{1000};
    std::atomic<bool> running_{false};
    std::map<std::string, uint64_t> last_price_by_symbol_;
    
    // Simple market maker parameters
    static constexpr uint64_t SPREAD_TICKS = 10;  // 10 tick spread
    static constexpr uint64_t ORDER_SIZE = 100;   // 100 shares per order
    
public:
    OrderBookManager() = default;
    
    void Start() {
        running_ = true;
        std::cout << "=== OrderBook Manager Started ===" << std::endl;
        printOrderBookStatus();
    }
    
    void Stop() {
        running_ = false;
        std::cout << "=== OrderBook Manager Stopped ===" << std::endl;
        printOrderBookStatus();
    }
    
    // Handle incoming market data and create synthetic orders
    KeepGoing OnMarketData(const Record& rec) {
        symbol_mappings_.OnRecord(rec);
        
        if (!running_) {
            return KeepGoing::Stop;
        }
        
        // Handle MBO (Market By Order) messages - direct order book updates
        if (const auto* mbo = rec.GetIf<MboMsg>()) {
            std::string symbol = symbol_mappings_[mbo->hd.instrument_id];
            
            // Handle different MBO actions
            switch (mbo->action) {
                case Action::Add: {
                    int64_t price_raw = static_cast<int64_t>(mbo->price);
                    int64_t price = price_raw / 1000000000;
                    char size = static_cast<char>(mbo->size);
                    uint64_t order_id = static_cast<uint64_t>(mbo->order_id);
                    bool is_buy = (mbo->side == Side::Bid);
                    
                    try {
                        order_book_.AddOrder(order_id, 1, is_buy, size, price);
                        std::cout << "[MBO-ADD] " << symbol << " Order " << order_id 
                                  << " " << (is_buy ? "BUY" : "SELL") 
                                  << " " << size << "@" << (price / 100.0) << std::endl;
                    } catch (const std::exception& e) {
                        // Order might already exist, which is fine for market data
                        std::cout << "[MBO-ADD-SKIP] Order " << order_id << " already exists" << std::endl;
                    }
                    break;
                }
                
                case Action::Cancel: {
                    uint64_t order_id = static_cast<uint64_t>(mbo->order_id);
                    try {
                        order_book_.CancelOrder(order_id);
                        std::cout << "[MBO-CANCEL] " << symbol << " Order " << order_id << " cancelled" << std::endl;
                    } catch (const std::exception& e) {
                        // Order might not exist, which is fine for market data
                        std::cout << "[MBO-CANCEL-SKIP] Order " << order_id << " not found" << std::endl;
                    }
                    break;
                }
                
                case Action::Modify: {
                    uint64_t order_id = static_cast<uint64_t>(mbo->order_id);
                    uint64_t new_price = static_cast<uint64_t>(mbo->price);
                    uint64_t new_size = static_cast<uint64_t>(mbo->size);
                    
                    try {
                        order_book_.ModifyOrder(order_id, new_size, new_price);
                        std::cout << "[MBO-MODIFY] " << symbol << " Order " << order_id 
                                  << " modified to " << new_size << "@" << (new_price / 100.0) << std::endl;
                    } catch (const std::exception& e) {
                        std::cout << "[MBO-MODIFY-SKIP] Order " << order_id << " modify failed: " << e.what() << std::endl;
                    }
                    break;
                }
                
                default:
                    break;
            }
            
            // Print periodic status updates
            static int mbo_count = 0;
            if (++mbo_count % 100 == 0) {
                printOrderBookStatus();
            }
        }
        
        // Handle trade messages - use them to generate synthetic order flow
        else if (const auto* trade = rec.GetIf<TradeMsg>()) {
            std::string symbol = symbol_mappings_[trade->hd.instrument_id];
            uint64_t price = static_cast<uint64_t>(trade->price);
            uint64_t size = static_cast<uint64_t>(trade->size);
            
            std::cout << "\n[TRADE] " << symbol 
                      << " - Price: " << (price / 100.0) 
                      << ", Size: " << size << std::endl;
            
            // Update last price for this symbol
            last_price_by_symbol_[symbol] = price;
            
            printOrderBookStatus();
        }
        
        // Handle quote/book updates for comparison
        else if (const auto* mbp1 = rec.GetIf<Mbp1Msg>()) {
            std::string symbol = symbol_mappings_[mbp1->hd.instrument_id];
            
            // Access the first level from the BidAskPair array
            const auto& level = mbp1->levels[0];
            
            std::cout << "\n[MARKET DATA] Quote for " << symbol 
                      << " - Bid: " << (level.bid_px / 100.0) 
                      << " (" << level.bid_sz << ")"
                      << ", Ask: " << (level.ask_px / 100.0) 
                      << " (" << level.ask_sz << ")" << std::endl;
        }
        
        return KeepGoing::Continue;
    }
    
private:
    void generateSyntheticOrders(const std::string& symbol, uint64_t trade_price, uint64_t trade_size) {
        try {
            // Generate some synthetic market making orders around the trade price
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> user_dist(1, 100);
            std::uniform_int_distribution<> size_dist(50, 200);
            
            // Place bid orders below the trade price
            uint64_t bid_price = trade_price - SPREAD_TICKS;
            uint64_t bid_size = size_dist(gen);
            uint64_t bid_order_id = order_id_counter_.fetch_add(1);
            
            order_book_.AddOrder(bid_order_id, user_dist(gen), true, bid_size, bid_price);
            
            // Place ask orders above the trade price  
            uint64_t ask_price = trade_price + SPREAD_TICKS;
            uint64_t ask_size = size_dist(gen);
            uint64_t ask_order_id = order_id_counter_.fetch_add(1);
            
            order_book_.AddOrder(ask_order_id, user_dist(gen), false, ask_size, ask_price);
            
            std::cout << "[SYNTHETIC] Added bid " << bid_order_id 
                      << " (" << bid_size << "@" << (bid_price/100.0) << ") "
                      << "and ask " << ask_order_id 
                      << " (" << ask_size << "@" << (ask_price/100.0) << ")" << std::endl;
                      
        } catch (const std::exception& e) {
            std::cout << "[ERROR] Failed to add synthetic orders: " << e.what() << std::endl;
        }
    }
    
    void updateOrderBookFromQuote(const std::string& symbol, 
                                 uint64_t bid_price, uint64_t bid_size,
                                 uint64_t ask_price, uint64_t ask_size) {
        try {
            // Occasionally add orders based on market quotes
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> decision(1, 10);
            
            if (decision(gen) <= 3) {  // 30% chance to add orders
                std::uniform_int_distribution<> user_dist(1, 100);
                
                uint64_t order_id = order_id_counter_.fetch_add(1);
                bool is_buy = decision(gen) <= 5;  // 50/50 buy/sell
                
                if (is_buy) {
                    // Add buy order slightly below market bid
                    uint64_t price = bid_price - 5;
                    order_book_.AddOrder(order_id, user_dist(gen), true, ORDER_SIZE, price);
                    std::cout << "[QUOTE-BASED] Added buy order " << order_id 
                              << " (" << ORDER_SIZE << "@" << (price/100.0) << ")" << std::endl;
                } else {
                    // Add sell order slightly above market ask
                    uint64_t price = ask_price + 5;
                    order_book_.AddOrder(order_id, user_dist(gen), false, ORDER_SIZE, price);
                    std::cout << "[QUOTE-BASED] Added sell order " << order_id 
                              << " (" << ORDER_SIZE << "@" << (price/100.0) << ")" << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cout << "[ERROR] Failed to add quote-based orders: " << e.what() << std::endl;
        }
    }
    
    void printOrderBookStatus() {
        try {
            std::cout << "\n=== Order Book Status ===" << std::endl;
            uint64_t best_bid = order_book_.GetBestBid();
            uint64_t best_ask = order_book_.GetBestAsk();
            
            if (best_bid > 0) {
                std::cout << "Best Bid: " << (best_bid / 100.0) << std::endl;
            } else {
                std::cout << "Best Bid: No bids" << std::endl;
            }
            
            if (best_ask > 0) {
                std::cout << "Best Ask: " << (best_ask / 100.0) << std::endl;
            } else {
                std::cout << "Best Ask: No asks" << std::endl;
            }
            
            std::cout << "Total Bid Volume: " << order_book_.GetTotalBidVolume() << std::endl;
            std::cout << "Total Ask Volume: " << order_book_.GetTotalAskVolume() << std::endl;
            std::cout << "=========================" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "[ERROR] Failed to print order book status: " << e.what() << std::endl;
        }
    }
};

void runLiveDataDemo() {
    std::cout << "\n=== Live Market Data Demo ===" << std::endl;
    std::cout << "This demo requires a valid DATABENTO_API_KEY environment variable." << std::endl;
    
    // Check if API key is available
    const char* api_key = std::getenv("DATABENTO_API_KEY");
    if (!api_key || strlen(api_key) == 0) {
        std::cout << "No DATABENTO_API_KEY found. Skipping live data demo." << std::endl;
        return;
    }
    
    try {
        OrderBookManager manager;
        
        auto client = LiveBuilder{}
                          .SetKeyFromEnv()
                          .SetDataset(Dataset::GlbxMdp3)
                          .BuildThreaded();
        
        auto handler = [&manager](const Record& rec) {
            return manager.OnMarketData(rec);
        };
        
        std::cout << "Starting live data stream for ES futures..." << std::endl;
        client.Subscribe({"ES.FUT"}, Schema::Trades, SType::Parent);
        client.Subscribe({"ES.FUT"}, Schema::Mbp1, SType::Parent);
        
        manager.Start();
        client.Start(handler);
        
        // Run for 30 seconds
        std::this_thread::sleep_for(std::chrono::seconds{30});
        
        manager.Stop();
        std::cout << "Live data demo completed." << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Live data demo error: " << e.what() << std::endl;
    }
}

void runHistoricalDataDemo() {
    std::cout << "\n=== Historical MBO Data Demo for ES Futures ===" << std::endl;
    
    // Check if API key is available
    const char* api_key = std::getenv("DATABENTO_API_KEY");
    if (!api_key || strlen(api_key) == 0) {
        std::cout << "No DATABENTO_API_KEY found. Skipping historical data demo." << std::endl;
        std::cout << "To run this demo, set your API key: export DATABENTO_API_KEY=your_key_here" << std::endl;
        return;
    }
    
    try {
        // Initialize cache and parameters
        DatabentoCache cache("databento_cache");
        
        // Historical data parameters
        std::string dataset = "GLBX.MDP3";
        std::string start_time = "2024-06-28T15:30";  // Active trading session (9:30 AM CT)
        std::string end_time = "2024-06-28T15:35";    // 5 minutes of data  
        std::vector<std::string> symbols = {"ESU4"};   // ES futures contract (September 2024)
        Schema schema = Schema::Mbo;
        
        // Generate cache key (for tracking what's cached)
        std::string cache_key = cache.generateCacheKey(dataset, start_time, end_time, symbols, schema);
        std::string cache_file_path = cache.getCacheFilePath(cache_key);
        
        std::cout << "Cache key: " << cache_key << std::endl;
        std::cout << "Cache file: " << cache_file_path << std::endl;
        cache.listCache();
        
        OrderBookManager manager;
        
        std::cout << "Fetching historical MBO data for ES S&P 500 futures..." << std::endl;
        std::cout << "Dataset: GLBX.MDP3 (CME Globex)" << std::endl;
        std::cout << "Schema: MBO (Market By Order) - Full order book depth" << std::endl;
        std::cout << "Symbol: ESU4 (E-mini S&P 500 futures September 2024)" << std::endl;
        std::cout << "Time range: " << start_time << " to " << end_time << " (UTC)" << std::endl;
        
        manager.Start();
        
        // Check if we have cached data
        if (cache.hasCachedData(cache_key)) { // Historical data never expires
            std::cout << "\n[CACHE] Loading data from cache file..." << std::endl;
            
            // Load DBN file directly from cache
            DbnFileStore dbn_store{cache_file_path};
            std::cout << "[CACHE] Successfully loaded cached DBN file" << std::endl;
            
            // Process the data from the DBN store
            std::cout << "Processing MBO messages..." << std::endl;
            
            auto metadata_callback = [](const Metadata& metadata) {
                std::cout << "Symbol metadata loaded for ES futures." << std::endl;
            };
            
            auto record_callback = [&manager](const Record& record) {
                return manager.OnMarketData(record);
            };
            
            // Replay the data
            dbn_store.Replay(metadata_callback, record_callback);
        } else {
            std::cout << "\n[API] Fetching fresh data from Databento API..." << std::endl;
            std::cout << "This will process real order book messages and build a live order book simulation." << std::endl;
            
            auto client = HistoricalBuilder{}.SetKeyFromEnv().Build();
            
            // Get data from API and save directly to cache file
            auto dbn_store = client.TimeseriesGetRangeToFile(
                dataset,               // CME Globex dataset
                {start_time, end_time}, // Time range
                symbols,               // ES S&P 500 futures (parent symbol)
                schema,                // Market By Order schema for full depth
                cache_file_path        // Save directly to cache file
            );
            
            std::cout << "[API] Successfully fetched and cached data to: " << cache_file_path << std::endl;
            
            // Process the data from the DBN store
            std::cout << "Processing MBO messages..." << std::endl;
            
            auto metadata_callback = [](const Metadata& metadata) {
                std::cout << "Symbol metadata loaded for ES futures." << std::endl;
            };
            
            auto record_callback = [&manager](const Record& record) {
                return manager.OnMarketData(record);
            };
            
            // Replay the data
            dbn_store.Replay(metadata_callback, record_callback);
        }
        
        manager.Stop();
        std::cout << "\n=== Historical MBO Data Demo Completed ===" << std::endl;
        std::cout << "Processed real ES futures order book data from CME Globex." << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Historical data demo error: " << e.what() << std::endl;
        std::cout << "\nPossible causes:" << std::endl;
        std::cout << "- Invalid API key or insufficient permissions" << std::endl;
        std::cout << "- Date range outside available data (try a more recent date)" << std::endl;
        std::cout << "- Network connectivity issues" << std::endl;
        std::cout << "- API rate limits exceeded" << std::endl;
        std::cout << "\nNote: MBO data requires appropriate subscription level." << std::endl;
    }
}

void runBasicOrderBookDemo() {
    std::cout << "\n=== Basic Order Book Demo (No External Data) ===" << std::endl;
    
    OrderBook book;
    
    std::cout << "1. Adding initial orders..." << std::endl;
    
    try {
        // Add some initial orders
        book.AddOrder(1001, 1, false, 100, 415025); // Sell 100 @ 4150.25
        book.AddOrder(1002, 1, false, 200, 415050); // Sell 200 @ 4150.50  
        book.AddOrder(1003, 1, false, 150, 415035); // Sell 150 @ 4150.35
        
        book.AddOrder(2001, 2, true, 80, 415000);   // Buy 80 @ 4150.00
        book.AddOrder(2002, 2, true, 120, 414975);  // Buy 120 @ 4149.75
        book.AddOrder(2003, 2, true, 90, 415010);   // Buy 90 @ 4150.10
        
        std::cout << "\n=== Order Book Status ===" << std::endl;
        std::cout << "Best Bid: " << (book.GetBestBid() / 100.0) << std::endl;
        std::cout << "Best Ask: " << (book.GetBestAsk() / 100.0) << std::endl;
        std::cout << "Total Bid Volume: " << book.GetTotalBidVolume() << std::endl;
        std::cout << "Total Ask Volume: " << book.GetTotalAskVolume() << std::endl;
        
        std::cout << "\n2. Adding aggressive order that crosses spread..." << std::endl;
        book.AddOrder(3001, 3, true, 250, 415040); // Buy 250 @ 4150.40
        
        std::cout << "\n=== Updated Order Book Status ===" << std::endl;
        std::cout << "Best Bid: " << (book.GetBestBid() / 100.0) << std::endl;
        std::cout << "Best Ask: " << (book.GetBestAsk() / 100.0) << std::endl;
        std::cout << "Total Bid Volume: " << book.GetTotalBidVolume() << std::endl;
        std::cout << "Total Ask Volume: " << book.GetTotalAskVolume() << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error in basic demo: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "OrderBook + Databento MBO Integration Demo" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "This example demonstrates integrating a custom C++ OrderBook" << std::endl;
    std::cout << "with real Market By Order (MBO) data from Databento's API." << std::endl;
    std::cout << "Focus: ES S&P 500 futures from CME Globex (GLBX.MDP3)" << std::endl;
    
    // Always run the basic demo
    runBasicOrderBookDemo();
    
    // Try to run live and historical demos if API key is available
    // runLiveDataDemo();  // Commented out to focus on historical data
   runHistoricalDataDemo();
    

    
    return 0;
}
