#include <iostream>
#include <vector>

#include <chrono>
#include <thread>


#include <memory>
#include <cstdlib>
#include <cstring>
#include <fstream>


// Include the OrderBook headers

#include "DatabentoCache.h"

#include "OrderBookManager.h"
#include "OrderImbalanceStrategy.h"
#include "DatabentoProcessor.h"
#include "IndicatorLogger.h"

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
        std::string(__FILE__).substr(0, std::string(__FILE__).find_last_of('/')) + "/../" + filename  // Relative to source file
    };
    
    std::ifstream file;
    std::string found_path;
    
    for (const auto& path : search_paths) {
        file.open(path);
        if (file.is_open()) {
            found_path = path;
            std::cout << "[ENV] Found .env file at: " << path << '\n';
            break;
        }
    }
    
    if (!file.is_open()) {
        std::cout << "[ENV] .env file not found in any of these locations:" << '\n';
        for (const auto& path : search_paths) {
            std::cout << "[ENV]   " << path << '\n';
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
            std::cout << "[ENV] Loaded: " << key << '\n';
        }
    }
    int x = 2;
    file.close();
}

void runLiveDataDemo() {
    std::cout << "\n=== Live Market Data Demo ===" << '\n';
    std::cout << "This demo requires a valid DATABENTO_API_KEY environment variable." << '\n';
    
    // Check if API key is available
    const char* api_key = std::getenv("DATABENTO_API_KEY");
    if (!api_key || strlen(api_key) == 0) {
        std::cout << "No DATABENTO_API_KEY found. Skipping live data demo." << '\n';
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
        
        std::cout << "Starting live data stream for ES futures..." << '\n';
        
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
        std::cout << "Live data demo completed." << '\n';
        
    } catch (const std::exception& e) {
        std::cout << "Live data demo error: " << e.what() << '\n';
    }
}

void runHistoricalDataDemo(double imbalance_threshold, size_t lookback_window,
                           double momentum_factor, double decay_factor,
                           double min_signal_for_trade, double stop_loss,
                           double max_daily_loss) {
    std::cout << "\n=== Historical MBO Data Demo for ES Futures ===" << '\n';
    
    // Check if API key is available
    const char* api_key = std::getenv("DATABENTO_API_KEY");
    bool has_api_key = (api_key && strlen(api_key) > 0);
    
    if (!has_api_key) {
        std::cout << "No DATABENTO_API_KEY found. Will use cached data only." << '\n';
    } else {
        std::cout << "API key found. Will use cached data or fetch if needed." << '\n';
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
        
        std::cout << "Cache key: " << cache_key << '\n';
        std::cout << "Cache file: " << cache_file_path << '\n';
        cache.listCache();
        
        // Initialize with 2ms slippage for historical data simulation
        const uint64_t STRATEGY_USER_ID = 9999;
        auto manager = std::make_shared<OrderBookManager>(STRATEGY_USER_ID);  // Track the strategy's user ID

        // Create and configure Databento processor
   
        auto databento_processor = std::make_shared<DatabentoProcessor>();
        manager->SetDataProcessor(databento_processor);
        
        // Initialize the manager after shared_ptr is fully constructed
        manager->InitializeAfterConstruction();
        
        std::cout << "Fetching historical MBO data for ES S&P 500 futures..." << '\n';
        std::cout << "Dataset: GLBX.MDP3 (CME Globex)" << '\n';
        std::cout << "Schema: MBO (Market By Order) - Full order book depth" << '\n';
        std::cout << "Symbol: ESU4 (E-mini S&P 500 futures December 2024)" << '\n';
        std::cout << "Time range: " << start_time << " to " << end_time << " (UTC)" << '\n';
        
        // Set up OrderImbalanceStrategy for historical data analysis
        std::cout << "\n=== Setting up OrderImbalanceStrategy for Historical Analysis ===" << '\n';
        auto order_imbalance_strategy = std::make_shared<OrderImbalanceStrategy>(
            "Historical_OrderImbalance",
            STRATEGY_USER_ID,
            imbalance_threshold,
            lookback_window
        );

        // Configure strategy parameters for historical analysis
        order_imbalance_strategy->SetMomentumFactor(momentum_factor);
        order_imbalance_strategy->SetDecayFactor(decay_factor);

        order_imbalance_strategy->EnableAutoTrading(true);
        order_imbalance_strategy->SetPortfolioManager(manager->GetPortfolioManager());
        order_imbalance_strategy->SetOrderClient(manager->GetClient());
        order_imbalance_strategy->SetSignalThreshold(min_signal_for_trade);  // Lower threshold for sensitivity
        order_imbalance_strategy->SetBaseQuantity(2);       // Moderate base quantity
        order_imbalance_strategy->SetMaxPosition(20);       // Limit max position size
        // Set risk management parameters
        manager->GetPortfolioManager()->SetStopLoss(stop_loss);
        manager->GetPortfolioManager()->SetMaxDailyLoss(max_daily_loss);

        manager->SetStrategy(order_imbalance_strategy);

        // Create and set up the indicator logger
        auto indicator_logger = std::make_shared<IndicatorLogger>("indicator_log.csv");
        order_imbalance_strategy->SetLogger(indicator_logger);
        
        std::cout << "OrderBookManager and DatabentoProcessor started successfully" << '\n';
        
        // Check if we have cached data
        if (cache.hasCachedData(cache_key)) { // Historical data never expires
            std::cout << "\n[CACHE] Loading data from cache file..." << '\n';
            
            // Load DBN file directly from cache
            DbnFileStore dbn_store{cache_file_path};
            std::cout << "[CACHE] Successfully loaded cached DBN file" << '\n';
            
            // Process the data from the DBN store
            std::cout << "Processing MBO messages..." << '\n';
            
            auto metadata_callback = [](const Metadata& metadata) {
                (void)metadata; // Suppress unused parameter warning
                std::cout << "Symbol metadata loaded for ES futures." << '\n';
                std::cout << "Metadata loaded successfully." << '\n';
            };
            
            int record_count = 0;
            auto record_callback = [&databento_processor, &record_count, &order_imbalance_strategy](const Record& record) -> KeepGoing {
                record_count++;
                if (record_count % 100 == 0) {
                    std::cout << "Processing record #" << record_count
                              << " (type: " << static_cast<int>(record.RType()) << ")" << '\n';
                }
                KeepGoing result = databento_processor->ProcessMarketData(record);
                if (record.RType() == RType::Mbo) {
                    order_imbalance_strategy->LogIndicators(record.Get<MboMsg>().ts_recv.time_since_epoch().count());
                }
                
                // Log first few records for debugging
                if (record_count <= 5) {
                    std::cout << "Record #" << record_count 
                              << " - Type: " << static_cast<int>(record.RType())
                              << " - Result: " << (result == KeepGoing::Continue ? "Continue" : "Stop") << '\n';
                }
                
                return result;
            };
            
            // Replay the data
            std::cout << "Starting replay of cached data..." << '\n';
            dbn_store.Replay(metadata_callback, record_callback);
            std::cout << "Replay completed. Total records processed: " << record_count << '\n';
        } else {
            if (!has_api_key) {
                std::cout << "\n[ERROR] No cached data found and no API key available." << '\n';
                std::cout << "Cannot fetch fresh data without API key." << '\n';
                std::cout << "Please either:" << '\n';
                std::cout << "1. Set DATABENTO_API_KEY environment variable" << '\n';
                std::cout << "2. Or run with an API key first to cache data" << '\n';
                manager->Stop();
                return;
            }
            
            std::cout << "\n[API] Fetching fresh data from Databento API..." << '\n';
            std::cout << "This will process real order book messages and build a live order book simulation." << '\n';
            
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
                
                std::cout << "[API] Successfully fetched and cached data to: " << cache_file_path << '\n';
                
                // Process the data from the DBN store
                std::cout << "Processing MBO messages..." << '\n';
                
                auto metadata_callback = [](const Metadata& metadata) {
                    (void)metadata; // Suppress unused parameter warning
                    std::cout << "Symbol metadata loaded for ES futures." << '\n';
                    std::cout << "Metadata loaded successfully." << '\n';
                };
                
                int record_count = 0;
                auto record_callback = [&databento_processor, &record_count, &order_imbalance_strategy](const Record& record) -> KeepGoing {
                    record_count++;
                    if (record_count % 100 == 0) {
                        std::cout << "Processing record #" << record_count
                                  << " (type: " << static_cast<int>(record.RType()) << ")" << '\n';
                    }
                    KeepGoing result = databento_processor->ProcessMarketData(record);
                    if (record.RType() == RType::Mbo) {
                        order_imbalance_strategy->LogIndicators(record.Get<MboMsg>().ts_recv.time_since_epoch().count());
                    }
                    
                    // Log first few records for debugging
                    if (record_count <= 5) {
                        std::cout << "Record #" << record_count 
                                  << " - Type: " << static_cast<int>(record.RType())
                                  << " - Result: " << (result == KeepGoing::Continue ? "Continue" : "Stop") << '\n';
                    }
                    
                    return result;
                };
                
                // Replay the data
                std::cout << "Starting replay of API data..." << '\n';
                dbn_store.Replay(metadata_callback, record_callback);
                std::cout << "Replay completed. Total records processed: " << record_count << '\n';
            } catch (const std::exception& e) {
                std::cerr << "\n[ERROR] Databento API error: " << e.what() << '\n';
                
                if (std::string(e.what()).find("symbology") != std::string::npos || 
                    std::string(e.what()).find("422") != std::string::npos) {
                    std::cout << "\n[INFO] Symbology error detected. This might be due to:" << '\n';
                    std::cout << "  - Expired futures contract (ESU4)" << '\n';
                    std::cout << "  - Dataset/symbol configuration issues" << '\n';
                    std::cout << "  - API key permissions" << '\n';
                    std::cout << "\n[SUGGESTION] Try using a more current futures contract or raw instrument IDs" << '\n';
                }
                
                std::cout << "\n[FALLBACK] Historical data demo skipped due to API error." << '\n';
                return;
            }
        }

        manager->GetPortfolioManager()->PrintPortfolioSummary();
        manager->Stop();
        std::cout << "\n=== Historical MBO Data Demo Completed ===" << '\n';
        std::cout << "Processed real ES futures order book data from CME Globex." << '\n';
        
    } catch (const std::exception& e) {
        std::cout << "Historical data demo error: " << e.what() << '\n';
        std::cout << "\nPossible causes:" << '\n';
        std::cout << "- Invalid API key or insufficient permissions" << '\n';
        std::cout << "- Date range outside available data (try a more recent date)" << '\n';
        std::cout << "- Network connectivity issues" << '\n';
        std::cout << "- API rate limits exceeded" << '\n';
        std::cout << "\nNote: MBO data requires appropriate subscription level." << '\n';
    }
}

void printHelp() {
    std::cout << "Usage: cpp_consumer_fetchcontent [options]\n"
              << "Options:\n"
              << "  --imbalance-threshold <value>  Set the imbalance threshold (default: 0.1)\n"
              << "  --lookback-window <value>      Set the lookback window (default: 30)\n"
              << "  --momentum-factor <value>      Set the momentum factor (default: 1.2)\n"
              << "  --decay-factor <value>         Set the decay factor (default: 0.98)\n"
              << "  --min-signal-for-trade <value> Set the minimum signal for a trade (default: 0.5)\n"
              << "  --stop-loss <value>            Set the stop-loss percentage (default: 0.02)\n"
              << "  --max-daily-loss <value>       Set the maximum daily loss (default: 1000.0)\n"
              << "  --help                         Show this help message\n";
}

int main(int argc, char* argv[]) {
    // Default parameters
    double imbalance_threshold = 0.1;
    size_t lookback_window = 30;
    double momentum_factor = 1.1;
    double decay_factor = 0.98;
    double min_signal_for_trade = 0.3;
    double stop_loss = 0.05;
    double max_daily_loss = 1000.0;

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            printHelp();
            return 0;
        } else if (arg == "--imbalance-threshold" && i + 1 < argc) {
            imbalance_threshold = std::stod(argv[++i]);
        } else if (arg == "--lookback-window" && i + 1 < argc) {
            lookback_window = std::stoi(argv[++i]);
        } else if (arg == "--momentum-factor" && i + 1 < argc) {
            momentum_factor = std::stod(argv[++i]);
        } else if (arg == "--decay-factor" && i + 1 < argc) {
            decay_factor = std::stod(argv[++i]);
        } else if (arg == "--min-signal-for-trade" && i + 1 < argc) {
            min_signal_for_trade = std::stod(argv[++i]);
        } else if (arg == "--stop-loss" && i + 1 < argc) {
            stop_loss = std::stod(argv[++i]);
        } else if (arg == "--max-daily-loss" && i + 1 < argc) {
            max_daily_loss = std::stod(argv[++i]);
        }
    }

    std::cout << "OrderBook + Databento MBO Integration Demo" << '\n';
    std::cout << "=========================================" << '\n';
    std::cout << "This example demonstrates:" << '\n';
    std::cout << "1. IClient interface implementation for order book operations" << '\n';
    std::cout << "2. DatabentoMboClient processing real Market By Order (MBO) data" << '\n';
    std::cout << "3. Integration with various market data feeds" << '\n';
    std::cout << "4. Proper encapsulation separating data processing from order book logic" << '\n';
    
    // Load environment variables from .env file
    loadEnvFile();
    
    // Check API key status
    const char* api_key = std::getenv("DATABENTO_API_KEY");
    if (api_key && strlen(api_key) > 0) {
        std::cout << "\n[API-KEY] Found Databento API key (length: " << strlen(api_key) << ")" << '\n';
    } else {
        std::cout << "\n[API-KEY] No Databento API key found" << '\n';
        std::cout << "Set DATABENTO_API_KEY environment variable to enable live data demos" << '\n';
    }
    
    
    // Try to run historical demo - will use cached data if no API key
    runHistoricalDataDemo(imbalance_threshold, lookback_window, momentum_factor,
                          decay_factor, min_signal_for_trade, stop_loss,
                          max_daily_loss);
    
    return 0;
}
