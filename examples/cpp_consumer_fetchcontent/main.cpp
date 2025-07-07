#include <iostream>
#include <vector>
#include <iomanip>
#include <chrono>
#include <thread>
#include <random>
#include <atomic>
#include <map>
#include <unordered_map>
#include <memory>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

// Include the OrderBook headers
#include "OrderBook.h"
#include "Trade.h"
#include "PriceLevel.h"
#include "Order.h"
#include "IClient.h"
#include "DatabentoCache.h"
#include "PortfolioManager.h"
#include "DatabentoMboClient.h"
#include "OrderBookManager.h"

// Include Databento headers
#include <databento/live.hpp>
#include <databento/historical.hpp>
#include <databento/symbol_map.hpp>
#include <databento/dbn.hpp>
#include <databento/dbn_file_store.hpp>

using namespace databento;


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
        // Initialize with 500 microseconds slippage for live data
        OrderBookManager manager(500000);  // 0.5ms slippage delay
        
        auto client = LiveBuilder{}
                          .SetKeyFromEnv()
                          .SetDataset(Dataset::GlbxMdp3)
                          .BuildThreaded();
        
        auto handler = [&manager](const Record& rec) {
            return manager.OnMarketData(rec);
        };
        
        std::cout << "Starting live data stream for ES futures..." << std::endl;
        
        // Subscribe to MBO (Market By Order) data for full order book reconstruction
        client.Subscribe({"ES.FUT"}, Schema::Mbo, SType::Parent);
        
        // Also subscribe to trades and quotes for comparison and market context
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
        
        // Historical data parameters - Using more recent contract and proper symbology
        std::string dataset = "GLBX.MDP3";
        std::string start_time = "2024-06-28T15:30";  // More recent date
        std::string end_time = "2024-06-28T15:35";    // 5 minutes of data
        std::vector<std::string> symbols = {"ESU4"};   // ES futures contract (June 2024)
        Schema schema = Schema::Mbo;
        
        // Generate cache key (for tracking what's cached)
        std::string cache_key = cache.generateCacheKey(dataset, start_time, end_time, symbols, schema);
        std::string cache_file_path = cache.getCacheFilePath(cache_key);
        
        std::cout << "Cache key: " << cache_key << std::endl;
        std::cout << "Cache file: " << cache_file_path << std::endl;
        cache.listCache();
        
        // Initialize with 2ms slippage for historical data simulation
        OrderBookManager manager(2000000);  // 2ms slippage delay
        
        std::cout << "Fetching historical MBO data for ES S&P 500 futures..." << std::endl;
        std::cout << "Dataset: GLBX.MDP3 (CME Globex)" << std::endl;
        std::cout << "Schema: MBO (Market By Order) - Full order book depth" << std::endl;
        std::cout << "Symbol: ESU4 (E-mini S&P 500 futures December 2024)" << std::endl;
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
                (void)metadata; // Suppress unused parameter warning
                std::cout << "Symbol metadata loaded for ES futures." << std::endl;
                std::cout << "Metadata loaded successfully." << std::endl;
            };
            
            int record_count = 0;
            auto record_callback = [&manager, &record_count](const Record& record) -> KeepGoing {
                record_count++;
                if (record_count % 100 == 0) {
                    std::cout << "Processing record #" << record_count 
                              << " (type: " << static_cast<int>(record.RType()) << ")" << std::endl;
                }
                KeepGoing result = manager.OnMarketData(record);
                
                // Log first few records for debugging
                if (record_count <= 5) {
                    std::cout << "Record #" << record_count 
                              << " - Type: " << static_cast<int>(record.RType())
                              << " - Result: " << (result == KeepGoing::Continue ? "Continue" : "Stop") << std::endl;
                }
                
                return result;
            };
            
            // Replay the data
            std::cout << "Starting replay of cached data..." << std::endl;
            dbn_store.Replay(metadata_callback, record_callback);
            std::cout << "Replay completed. Total records processed: " << record_count << std::endl;
        } else {
            std::cout << "\n[API] Fetching fresh data from Databento API..." << std::endl;
            std::cout << "This will process real order book messages and build a live order book simulation." << std::endl;
            
            try {
                auto client = HistoricalBuilder{}.SetKeyFromEnv().Build();
                
                // Get data from API and save directly to cache file
                auto dbn_store = client.TimeseriesGetRangeToFile(
                    dataset,               // CME Globex dataset
                    {start_time, end_time}, // Time range
                    symbols,               // ES S&P 500 futures
                    schema,                // Market By Order schema for full depth
                    cache_file_path        // Save directly to cache file
                );
                
                std::cout << "[API] Successfully fetched and cached data to: " << cache_file_path << std::endl;
                
                // Process the data from the DBN store
                std::cout << "Processing MBO messages..." << std::endl;
                
                auto metadata_callback = [](const Metadata& metadata) {
                    (void)metadata; // Suppress unused parameter warning
                    std::cout << "Symbol metadata loaded for ES futures." << std::endl;
                    std::cout << "Metadata loaded successfully." << std::endl;
                };
                
                int record_count = 0;
                auto record_callback = [&manager, &record_count](const Record& record) -> KeepGoing {
                    record_count++;
                    if (record_count % 100 == 0) {
                        std::cout << "Processing record #" << record_count 
                                  << " (type: " << static_cast<int>(record.RType()) << ")" << std::endl;
                    }
                    KeepGoing result = manager.OnMarketData(record);
                    
                    // Log first few records for debugging
                    if (record_count <= 5) {
                        std::cout << "Record #" << record_count 
                                  << " - Type: " << static_cast<int>(record.RType())
                                  << " - Result: " << (result == KeepGoing::Continue ? "Continue" : "Stop") << std::endl;
                    }
                    
                    return result;
                };
                
                // Replay the data
                std::cout << "Starting replay of API data..." << std::endl;
                dbn_store.Replay(metadata_callback, record_callback);
                std::cout << "Replay completed. Total records processed: " << record_count << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "\n[ERROR] Databento API error: " << e.what() << std::endl;
                
                if (std::string(e.what()).find("symbology") != std::string::npos || 
                    std::string(e.what()).find("422") != std::string::npos) {
                    std::cout << "\n[INFO] Symbology error detected. This might be due to:" << std::endl;
                    std::cout << "  - Expired futures contract (ESU4)" << std::endl;
                    std::cout << "  - Dataset/symbol configuration issues" << std::endl;
                    std::cout << "  - API key permissions" << std::endl;
                    std::cout << "\n[SUGGESTION] Try using a more current futures contract or raw instrument IDs" << std::endl;
                }
                
                std::cout << "\n[FALLBACK] Historical data demo skipped due to API error." << std::endl;
                return;
            }
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
    std::cout << "\n=== Simple P&L Demo for Portfolio Tracking ===" << std::endl;
    
    // Create a fresh order book instance
    auto order_book = std::make_shared<OrderBook>();
    
    // Track user 1000 for this demo
    uint64_t tracked_user_id = 1000;
    auto client = std::make_shared<DatabentoMboClient>(1, "Demo Client", order_book, tracked_user_id, 100000);
    
    // Register the client with the OrderBook
    order_book->RegisterClient(client);
    
    auto portfolio = client->GetPortfolioManager();
    if (portfolio) {
        portfolio->EnablePeriodicSnapshots(100000000); // 100ms intervals for demo
    }

    try {
        std::cout << "\n=== Scenario 1: +$100 Profit ===" << std::endl;
        std::cout << "Tracked user buys 100 contracts at $50.00 and sells at $51.00" << std::endl;
        
        // Step 1a: Tracked user places buy order at $50.00
        std::cout << "1. Tracked user places buy order: 100 @ $50.00" << std::endl;
        client->SubmitOrder(tracked_user_id, true, 100, 5000);  // Buy 100 @ $50.00
        
        // Step 1b: Someone sells to the tracked user
        std::cout << "2. Market participant sells to tracked user at $50.00" << std::endl;
        client->SubmitOrder(99, false, 100, 5000);  // Sell 100 @ $50.00 (fills tracked user's buy)
        
        // Step 1c: Tracked user places sell order at $51.00  
        std::cout << "3. Tracked user places sell order: 100 @ $51.00" << std::endl;
        client->SubmitOrder(tracked_user_id, false, 100, 5100);  // Sell 100 @ $51.00
        
        // Step 1d: Someone buys from the tracked user
        std::cout << "4. Market participant buys from tracked user at $51.00" << std::endl;
        client->SubmitOrder(98, true, 100, 5100);  // Buy 100 @ $51.00 (fills tracked user's sell)
        
        std::cout << "\n=== Scenario 2: -$100 Loss ===" << std::endl;
        std::cout << "Tracked user buys 100 contracts at $51.00 and sells at $50.00" << std::endl;
        
        // Step 2a: Tracked user places buy order at $51.00
        std::cout << "5. Tracked user places buy order: 100 @ $51.00" << std::endl;
        client->SubmitOrder(tracked_user_id, true, 100, 5100);  // Buy 100 @ $51.00
        
        // Step 2b: Someone sells to the tracked user
        std::cout << "6. Market participant sells to tracked user at $51.00" << std::endl;
        client->SubmitOrder(97, false, 100, 5100);  // Sell 100 @ $51.00 (fills tracked user's buy)
        
        // Step 2c: Tracked user places sell order at $50.00
        std::cout << "7. Tracked user places sell order: 100 @ $50.00" << std::endl;
        client->SubmitOrder(tracked_user_id, false, 100, 5000);  // Sell 100 @ $50.00
        
        // Step 2d: Someone buys from the tracked user
        std::cout << "8. Market participant buys from tracked user at $50.00" << std::endl;
        client->SubmitOrder(96, true, 100, 5000);  // Buy 100 @ $50.00 (fills tracked user's sell)
        
        std::cout << "\n=== Scenario 3: +$100 Profit (Return to Zero) ===" << std::endl;
        std::cout << "Tracked user buys 100 contracts at $50.00 and sells at $51.00" << std::endl;
        
        // Step 3a: Tracked user places buy order at $50.00
        std::cout << "9. Tracked user places buy order: 100 @ $50.00" << std::endl;
        client->SubmitOrder(tracked_user_id, true, 100, 5000);  // Buy 100 @ $50.00
        
        // Step 3b: Someone sells to the tracked user
        std::cout << "10. Market participant sells to tracked user at $50.00" << std::endl;
        client->SubmitOrder(95, false, 100, 5000);  // Sell 100 @ $50.00 (fills tracked user's buy)
        
        // Step 3c: Tracked user places sell order at $51.00
        std::cout << "11. Tracked user places sell order: 100 @ $51.00" << std::endl;
        client->SubmitOrder(tracked_user_id, false, 100, 5100);  // Sell 100 @ $51.00
        
        // Step 3d: Someone buys from the tracked user
        std::cout << "12. Market participant buys from tracked user at $51.00" << std::endl;
        client->SubmitOrder(94, true, 100, 5100);  // Buy 100 @ $51.00 (fills tracked user's sell)
        
        // Force a final snapshot
        if (portfolio) {
            portfolio->ForceSnapshot();
        }
        
        // Show final portfolio summary
        if (portfolio) {
            std::cout << "\n=== Final Portfolio Summary ===" << std::endl;
            portfolio->PrintPortfolioSummary();
        }
        
        // Unregister and shutdown the client
        order_book->UnregisterClient(client->GetClientId());
        
    } catch (const std::exception& e) {
        std::cerr << "Error in demo: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "OrderBook + Databento MBO Integration Demo" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "This example demonstrates:" << std::endl;
    std::cout << "1. IClient interface implementation for order book operations" << std::endl;
    std::cout << "2. DatabentoMboClient processing real Market By Order (MBO) data" << std::endl;
    std::cout << "3. Integration with various market data feeds" << std::endl;
    std::cout << "4. Proper encapsulation separating data processing from order book logic" << std::endl;
    
    // Check API key status
    const char* api_key = std::getenv("DATABENTO_API_KEY");
    if (api_key && strlen(api_key) > 0) {
        std::cout << "\n[API-KEY] Found Databento API key (length: " << strlen(api_key) << ")" << std::endl;
    } else {
        std::cout << "\n[API-KEY] No Databento API key found" << std::endl;
        std::cout << "Set DATABENTO_API_KEY environment variable to enable live data demos" << std::endl;
    }
    
    // Always run the basic demo to show client interface and single-user portfolio tracking
    runBasicOrderBookDemo();
    
    // Try to run historical demo if API key is available  
    runHistoricalDataDemo();
    
    return 0;
}
