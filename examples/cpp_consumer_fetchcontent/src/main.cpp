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
#include "OrderBookManager.h"
#include "OrderImbalanceStrategy.h"
#include "DatabentoProcessor.h"

// Include Databento headers
#include <databento/live.hpp>
#include <databento/historical.hpp>
#include <databento/symbol_map.hpp>
#include <databento/dbn.hpp>
#include <databento/dbn_file_store.hpp>

using namespace databento;

// Function to load environment variables from .env file
void loadEnvFile(const std::string& filename = ".env") {
    std::vector<std::string> search_paths = {
        filename,                                           // Current working directory
        "../" + filename,                                   // Parent directory (for build/)
        "../../" + filename,                               // For nested build directories
        std::string(__FILE__).substr(0, std::string(__FILE__).find_last_of("/")) + "/../" + filename  // Relative to source file
    };
    
    std::ifstream file;
    std::string found_path;
    
    for (const auto& path : search_paths) {
        file.open(path);
        if (file.is_open()) {
            found_path = path;
            std::cout << "[ENV] Found .env file at: " << path << std::endl;
            break;
        }
    }
    
    if (!file.is_open()) {
        std::cout << "[ENV] .env file not found in any of these locations:" << std::endl;
        for (const auto& path : search_paths) {
            std::cout << "[ENV]   " << path << std::endl;
        }
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        
        // Find the = separator
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;
        
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        
        // Remove leading/trailing whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        // Set environment variable
        if (setenv(key.c_str(), value.c_str(), 1) == 0) {
            std::cout << "[ENV] Loaded: " << key << std::endl;
        }
    }
    file.close();
}

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
        auto manager = std::make_shared<OrderBookManager>(500000);  // 0.5ms slippage delay
        
        // Create and configure Databento processor
        auto databento_processor = std::make_shared<DatabentoProcessor>();
        manager->SetDataProcessor(databento_processor);
        
        // Initialize the manager after shared_ptr is fully constructed
        manager->InitializeAfterConstruction();
        
        auto client = LiveBuilder{}
                          .SetKeyFromEnv()
                          .SetDataset(Dataset::GlbxMdp3)
                          .BuildThreaded();
        
        auto handler = [&databento_processor](const Record& rec) -> KeepGoing {
            return databento_processor->ProcessMarketData(rec);
        };
        
        std::cout << "Starting live data stream for ES futures..." << std::endl;
        
        // Subscribe to MBO (Market By Order) data for full order book reconstruction
        client.Subscribe({"ES.FUT"}, Schema::Mbo, SType::Parent);
        
        // Also subscribe to trades and quotes for comparison and market context
        client.Subscribe({"ES.FUT"}, Schema::Trades, SType::Parent);
        client.Subscribe({"ES.FUT"}, Schema::Mbp1, SType::Parent);
        
        manager->Start();

        client.Start(handler);
        
        // Run for 30 seconds
        std::this_thread::sleep_for(std::chrono::seconds{30});
        

        manager->Stop();
        std::cout << "Live data demo completed." << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Live data demo error: " << e.what() << std::endl;
    }
}

void runHistoricalDataDemo() {
    std::cout << "\n=== Historical MBO Data Demo for ES Futures ===" << std::endl;
    
    // Check if API key is available
    const char* api_key = std::getenv("DATABENTO_API_KEY");
    bool has_api_key = (api_key && strlen(api_key) > 0);
    
    if (!has_api_key) {
        std::cout << "No DATABENTO_API_KEY found. Will use cached data only." << std::endl;
    } else {
        std::cout << "API key found. Will use cached data or fetch if needed." << std::endl;
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
        
        // Initialize with 2ms slippage for historical data simulation, tracking user 1000
        uint64_t tracked_user_id = 1000;
        uint64_t slippage_delay_ns_ = 2000000; // 2ms
        auto manager = std::make_shared<OrderBookManager>(slippage_delay_ns_, tracked_user_id);  // 2ms slippage delay, user 1000

        // Create and configure Databento processor
   
        auto databento_processor = std::make_shared<DatabentoProcessor>();
        manager->SetDataProcessor(databento_processor);
        
        // Initialize the manager after shared_ptr is fully constructed
        manager->InitializeAfterConstruction();
        
        std::cout << "Fetching historical MBO data for ES S&P 500 futures..." << std::endl;
        std::cout << "Dataset: GLBX.MDP3 (CME Globex)" << std::endl;
        std::cout << "Schema: MBO (Market By Order) - Full order book depth" << std::endl;
        std::cout << "Symbol: ESU4 (E-mini S&P 500 futures December 2024)" << std::endl;
        std::cout << "Time range: " << start_time << " to " << end_time << " (UTC)" << std::endl;
        
        // Set up OrderImbalanceStrategy for historical data analysis
        std::cout << "\n=== Setting up OrderImbalanceStrategy for Historical Analysis ===" << std::endl;
        auto client = manager->GetClient();

        
        std::cout << "OrderBookManager and DatabentoProcessor started successfully" << std::endl;
        
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
            auto record_callback = [&databento_processor, &record_count](const Record& record) -> KeepGoing {
                record_count++;
                if (record_count % 100 == 0) {
                    std::cout << "Processing record #" << record_count 
                              << " (type: " << static_cast<int>(record.RType()) << ")" << std::endl;
                }
                KeepGoing result = databento_processor->ProcessMarketData(record);
                
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
            if (!has_api_key) {
                std::cout << "\n[ERROR] No cached data found and no API key available." << std::endl;
                std::cout << "Cannot fetch fresh data without API key." << std::endl;
                std::cout << "Please either:" << std::endl;
                std::cout << "1. Set DATABENTO_API_KEY environment variable" << std::endl;
                std::cout << "2. Or run with an API key first to cache data" << std::endl;
                manager->Stop();
                return;
            }
            
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
                auto record_callback = [&databento_processor, &record_count](const Record& record) -> KeepGoing {
                    record_count++;
                    if (record_count % 100 == 0) {
                        std::cout << "Processing record #" << record_count 
                                  << " (type: " << static_cast<int>(record.RType()) << ")" << std::endl;
                    }
                    KeepGoing result = databento_processor->ProcessMarketData(record);
                    
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

        manager->GetPortfolioManager()->PrintPortfolioSummary();
        manager->Stop();
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

int main() {
    std::cout << "OrderBook + Databento MBO Integration Demo" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "This example demonstrates:" << std::endl;
    std::cout << "1. IClient interface implementation for order book operations" << std::endl;
    std::cout << "2. DatabentoMboClient processing real Market By Order (MBO) data" << std::endl;
    std::cout << "3. Integration with various market data feeds" << std::endl;
    std::cout << "4. Proper encapsulation separating data processing from order book logic" << std::endl;
    
    // Load environment variables from .env file
    loadEnvFile();
    
    // Check API key status
    const char* api_key = std::getenv("DATABENTO_API_KEY");
    if (api_key && strlen(api_key) > 0) {
        std::cout << "\n[API-KEY] Found Databento API key (length: " << strlen(api_key) << ")" << std::endl;
    } else {
        std::cout << "\n[API-KEY] No Databento API key found" << std::endl;
        std::cout << "Set DATABENTO_API_KEY environment variable to enable live data demos" << std::endl;
    }
    
    
    // Try to run historical demo - will use cached data if no API key
    runHistoricalDataDemo();
    
    return 0;
}
