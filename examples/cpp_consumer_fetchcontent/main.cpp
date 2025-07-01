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

// Include the OrderBook headers
#include "OrderBook.h"
#include "Trade.h"
#include "PriceLevel.h"
#include "Order.h"
#include "IClient.h"
#include "DatabentoCache.h"
// Include Databento headers
#include <databento/live.hpp>
#include <databento/historical.hpp>
#include <databento/symbol_map.hpp>
#include <databento/dbn.hpp>
#include <databento/dbn_file_store.hpp>

using namespace databento;

/**
 * @brief Databento MBO Client implementing the IClient interface
 * 
 * This client is responsible for:
 * 1. Processing Databento MBO (Market By Order) messages
 * 2. Translating MBO actions to order book operations
 * 3. Managing symbol mappings and market data
 * 4. Providing callbacks for order book events
 */
class DatabentoMboClient : public IClient {
private:
    std::shared_ptr<OrderBook> order_book_;
    uint64_t client_id_;
    std::string client_name_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> next_order_id_{1000};
    
    // Symbol mapping for Databento instrument IDs
    PitSymbolMap symbol_mappings_;
    
    // Track last prices for market making
    std::unordered_map<std::string, uint64_t> last_price_by_symbol_;
    
    // Order ID mapping between Databento and internal IDs
    std::unordered_map<uint64_t, uint64_t> databento_to_internal_order_id_;
    std::unordered_map<uint64_t, uint64_t> internal_to_databento_order_id_;

public:
    DatabentoMboClient(uint64_t client_id, const std::string& name, std::shared_ptr<OrderBook> order_book)
        : order_book_(order_book), client_id_(client_id), client_name_(name) {
    }

    // ========== IClient Interface Implementation ==========
    
    uint64_t SubmitOrder(uint64_t user_id, bool is_buy, uint64_t quantity, uint64_t price) override {
        if (!running_ || !order_book_) {
            return 0;
        }
        
        uint64_t order_id = GenerateOrderId();
        try {
            order_book_->AddOrder(order_id, user_id, is_buy, quantity, price);
            return order_id;
        } catch (const std::exception& e) {
            return 0;
        }
    }

    bool CancelOrder(uint64_t order_id) override {
        if (!running_ || !order_book_) {
            return false;
        }
        
        try {
            order_book_->CancelOrder(order_id);
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }

    bool ModifyOrder(uint64_t order_id, uint64_t new_quantity, uint64_t new_price) override {
        if (!running_ || !order_book_) {
            return false;
        }
        
        try {
            order_book_->ModifyOrder(order_id, new_quantity, new_price);
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }

    uint64_t GetBestBid() const override {
        if (!order_book_) return 0;
        return order_book_->GetBestBid();
    }

    uint64_t GetBestAsk() const override {
        if (!order_book_) return 0;
        return order_book_->GetBestAsk();
    }

    uint64_t GetTotalBidVolume() const override {
        if (!order_book_) return 0;
        return order_book_->GetTotalBidVolume();
    }

    uint64_t GetTotalAskVolume() const override {
        if (!order_book_) return 0;
        return order_book_->GetTotalAskVolume();
    }

    uint64_t GetSpread() const override {
        uint64_t bid = GetBestBid();
        uint64_t ask = GetBestAsk();
        if (bid == 0 || ask == 0) return 0;
        return ask - bid;
    }

    uint64_t GetMidPrice() const override {
        uint64_t bid = GetBestBid();
        uint64_t ask = GetBestAsk();
        if (bid == 0 || ask == 0) return 0;
        return (bid + ask) / 2;
    }

    // ========== IClient Event Handlers ==========

    void OnTradeExecuted(const Trade& trade) override {
        std::cout << "[CLIENT-TRADE] " << trade.aggressor_order_id << " x " << trade.resting_order_id 
                  << " @ " << std::fixed << std::setprecision(2) << (trade.price / 100.0) 
                  << " size=" << trade.quantity << std::endl;
    }

    void OnOrderAcknowledged(uint64_t order_id, uint64_t user_id) override {
        (void)order_id; (void)user_id; // Suppress unused parameter warnings
    }

    void OnOrderCancelled(uint64_t order_id, uint64_t user_id) override {
        (void)order_id; (void)user_id; // Suppress unused parameter warnings
    }

    void OnOrderModified(uint64_t order_id, uint64_t user_id, uint64_t new_quantity, uint64_t new_price) override {
        (void)order_id; (void)user_id; (void)new_quantity; (void)new_price; // Suppress unused parameter warnings
    }

    void OnOrderRejected(uint64_t order_id, uint64_t user_id, const std::string& reason) override {
        (void)user_id; // Suppress unused parameter warning
        if (reason.find("already exists") == std::string::npos && 
            reason.find("not found") == std::string::npos) {
            std::cout << "[CLIENT-REJECT] Order " << order_id << " rejected: " << reason << std::endl;
        }
    }

    void OnTopOfBookUpdate(uint64_t best_bid, uint64_t best_ask, uint64_t bid_volume, uint64_t ask_volume) override {
        std::cout << "[CLIENT-TOB] Bid=" << std::fixed << std::setprecision(2) << (best_bid / 100.0) << "(" << bid_volume << ")"
                  << ", Ask=" << std::fixed << std::setprecision(2) << (best_ask / 100.0) << "(" << ask_volume << ")"
                  << ", Mid=" << std::fixed << std::setprecision(2) << (GetMidPrice() / 100.0)
                  << ", Spread=" << std::fixed << std::setprecision(2) << (GetSpread() / 100.0) << std::endl;
    }

    void Initialize() override {
        running_ = true;
        std::cout << "[CLIENT] " << client_name_ << " initialized (ID: " << client_id_ << ")" << std::endl;
        PrintOrderBookStatus();
    }

    void Shutdown() override {
        running_ = false;
        std::cout << "[CLIENT] " << client_name_ << " shutting down" << std::endl;
        PrintOrderBookStatus();
    }

    uint64_t GetClientId() const override {
        return client_id_;
    }

    std::string GetClientName() const override {
        return client_name_;
    }

    // ========== Databento MBO Message Processing ==========

    KeepGoing ProcessMarketData(const Record& rec) {
        symbol_mappings_.OnRecord(rec);
        
        if (!running_) {
            return KeepGoing::Stop;
        }
        
        // Handle different record types based on RType
        switch (rec.RType()) {
            case RType::Mbo: {
                const auto& mbo = rec.Get<MboMsg>();
                return ProcessMboMessage(mbo);
            }
            case RType::Mbp0: {
                const auto& trade = rec.Get<TradeMsg>();
                return ProcessTradeMessage(trade);
            }
            case RType::Mbp1:
            case RType::Bbo1M:
            case RType::Bbo1S: {
                const auto& mbp1 = rec.Get<Mbp1Msg>();
                return ProcessQuoteMessage(mbp1);
            }
            default:
                // Ignore other message types
                break;
        }
        
        return KeepGoing::Continue;
    }

private:
    uint64_t GenerateOrderId() {
        return next_order_id_.fetch_add(1);
    }

    KeepGoing ProcessMboMessage(const MboMsg& mbo) {
        std::string symbol = symbol_mappings_[mbo.hd.instrument_id];
        
        // Debug: Print symbol mapping info
        static int debug_count = 0;
        if (debug_count < 10) {
            std::cout << "[DEBUG] Instrument ID: " << mbo.hd.instrument_id << " -> Symbol: '" << symbol << "'" << std::endl;
            debug_count++;
        }
        
        // If symbol mapping fails, try to use a default symbol for ES futures
        if (symbol.empty()) {
            // For demonstration purposes, assume this is ES futures data
            symbol = "ESU4";  // Use the expected symbol
        }
        
        // Handle different MBO actions
        switch (mbo.action) {
            case Action::Add: {
                int64_t price_raw = static_cast<int64_t>(mbo.price);
                // For ES futures: Databento prices are in nano-precision, convert to index points
                // ES futures trade in 0.25 point increments (e.g., 5432.25, 5432.50, etc.)
                double price_points = static_cast<double>(price_raw) / 1000000000.0;
                int64_t price_for_orderbook = static_cast<int64_t>(price_points * 100); // Store as hundredths for precision
                uint64_t size = static_cast<uint64_t>(mbo.size);
                uint64_t order_id = static_cast<uint64_t>(mbo.order_id);
                bool is_buy = (mbo.side == Side::Bid);
                
                try {
                    order_book_->AddOrder(order_id, 1, is_buy, size, price_for_orderbook);
                    std::cout << "[MBO-ADD] " << symbol << " Order " << order_id 
                              << " " << (is_buy ? "BUY" : "SELL") 
                              << " " << size << "@" << std::fixed << std::setprecision(2) << price_points << std::endl;
                } catch (const std::exception& e) {
                    // Order might already exist, which is fine for market data
                    std::cout << "[MBO-ADD-SKIP] Order " << order_id << " already exists" << std::endl;
                }
                break;
            }
            
            case Action::Cancel: {
                uint64_t order_id = static_cast<uint64_t>(mbo.order_id);
                try {
                    order_book_->CancelOrder(order_id);
                    std::cout << "[MBO-CANCEL] " << symbol << " Order " << order_id << " cancelled" << std::endl;
                } catch (const std::exception& e) {
                    // Order might not exist, which is fine for market data
                    std::cout << "[MBO-CANCEL-SKIP] Order " << order_id << " not found" << std::endl;
                }
                break;
            }
            
            case Action::Modify: {
                uint64_t order_id = static_cast<uint64_t>(mbo.order_id);
                // Fix price conversion for ES futures
                double new_price_points = static_cast<double>(mbo.price) / 1000000000.0;
                int64_t new_price_for_orderbook = static_cast<int64_t>(new_price_points * 100);
                uint64_t new_size = static_cast<uint64_t>(mbo.size);
                
                try {
                    order_book_->ModifyOrder(order_id, new_size, new_price_for_orderbook);
                    std::cout << "[MBO-MODIFY] " << symbol << " Order " << order_id 
                              << " modified to " << new_size << "@" << std::fixed << std::setprecision(2) << new_price_points << std::endl;
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
        
        return KeepGoing::Continue;
    }

    KeepGoing ProcessTradeMessage(const TradeMsg& trade) {
        std::string symbol = symbol_mappings_[trade.hd.instrument_id];
        
        if (symbol.empty()) {
            return KeepGoing::Continue;
        }
        
        uint64_t price = static_cast<uint64_t>(trade.price / 1000000000);
        uint64_t size = static_cast<uint64_t>(trade.size);
        
        std::cout << "\n[TRADE] " << symbol 
                  << " - Price: " << (price / 100.0) 
                  << ", Size: " << size << std::endl;
        
        // Update last price for this symbol
        last_price_by_symbol_[symbol] = price;
        
        printOrderBookStatus();
        
        return KeepGoing::Continue;
    }

    KeepGoing ProcessQuoteMessage(const Mbp1Msg& mbp1) {
        std::string symbol = symbol_mappings_[mbp1.hd.instrument_id];
        
        if (symbol.empty()) {
            return KeepGoing::Continue;
        }
        
        // Access the first level from the BidAskPair array
        const auto& level = mbp1.levels[0];
        
        std::cout << "\n[MARKET DATA] Quote for " << symbol 
                  << " - Bid: " << (level.bid_px / 1e9) 
                  << " (" << level.bid_sz << ")"
                  << ", Ask: " << (level.ask_px / 1e9) 
                  << " (" << level.ask_sz << ")" << std::endl;
        
        return KeepGoing::Continue;
    }

    void PrintOrderBookStatus() {
        if (!order_book_) return;
        
        std::cout << "\n=== Order Book Status ===" << std::endl;
        
        uint64_t best_bid = GetBestBid();
        uint64_t best_ask = GetBestAsk();
        
        if (best_bid > 0) {
            std::cout << "Best Bid: " << std::fixed << std::setprecision(2) << (best_bid / 100.0) << std::endl;
        } else {
            std::cout << "Best Bid: No bids" << std::endl;
        }
        
        if (best_ask > 0) {
            std::cout << "Best Ask: " << std::fixed << std::setprecision(2) << (best_ask / 100.0) << std::endl;
        } else {
            std::cout << "Best Ask: No asks" << std::endl;
        }
        
        if (best_bid > 0 && best_ask > 0) {
            std::cout << "Spread: " << std::fixed << std::setprecision(2) << (GetSpread() / 100.0) << std::endl;
            std::cout << "Mid Price: " << std::fixed << std::setprecision(2) << (GetMidPrice() / 100.0) << std::endl;
        }
        
        std::cout << "Total Bid Volume: " << GetTotalBidVolume() << std::endl;
        std::cout << "Total Ask Volume: " << GetTotalAskVolume() << std::endl;
        std::cout << "=========================" << std::endl;
    }
    
    // Alias for compatibility with original code style
    void printOrderBookStatus() {
        PrintOrderBookStatus();
    }
};


/**
 * @brief Manager class to coordinate Databento data with the client
 * 
 * This class serves as a bridge between Databento data feeds and the client,
 * handling the data flow and client lifecycle management.
 */
class OrderBookManager {
private:
    std::shared_ptr<DatabentoMboClient> client_;
    std::shared_ptr<OrderBook> order_book_;
    
public:
    OrderBookManager() {
        order_book_ = std::make_shared<OrderBook>();
        client_ = std::make_shared<DatabentoMboClient>(1, "Databento MBO Client", order_book_);
        // Register the client with the order book for callbacks
        order_book_->RegisterClient(client_);
    }
    
    void Start() {
        // Client is already initialized via RegisterClient
    }
    
    void Stop() {
        // Client will be shutdown via UnregisterClient in OrderBook destructor
        order_book_->UnregisterClient(client_->GetClientId());
    }
    
    // Bridge method for Databento callbacks
    KeepGoing OnMarketData(const Record& record) {
        return client_->ProcessMarketData(record);
    }
    
    // Provide access to client for additional operations
    std::shared_ptr<IClient> GetClient() {
        return client_;
    }
};

void runBasicOrderBookDemo(OrderBookManager& manager) {
    std::cout << "\n=== Basic Order Book Demo (No External Data) ===" << std::endl;
    
    // Start the manager
    manager.Start();
    
    // Get client interface
    auto client = manager.GetClient();
    if (!client) {
        std::cerr << "Failed to get client from manager" << std::endl;
        return;
    }
    
    std::cout << "1. Adding initial orders via client interface..." << std::endl;
    
    try {
        // Submit orders through the client interface
        client->SubmitOrder(1, false, 100, 415025); // Sell 100 @ 4150.25
        client->SubmitOrder(1, false, 200, 415050); // Sell 200 @ 4150.50  
        client->SubmitOrder(1, false, 150, 415035); // Sell 150 @ 4150.35
        
        client->SubmitOrder(2, true, 80, 415000);   // Buy 80 @ 4150.00
        client->SubmitOrder(2, true, 120, 414975);  // Buy 120 @ 4149.75
        client->SubmitOrder(2, true, 90, 415010);   // Buy 90 @ 4150.10
        
        std::cout << "\n=== Order Book Status via Client ===" << std::endl;
        std::cout << "Best Bid: " << std::fixed << std::setprecision(2) << (client->GetBestBid() / 100.0) << std::endl;
        std::cout << "Best Ask: " << std::fixed << std::setprecision(2) << (client->GetBestAsk() / 100.0) << std::endl;
        std::cout << "Mid Price: " << std::fixed << std::setprecision(2) << (client->GetMidPrice() / 100.0) << std::endl;
        std::cout << "Spread: " << std::fixed << std::setprecision(2) << (client->GetSpread() / 100.0) << std::endl;
        std::cout << "Total Bid Volume: " << client->GetTotalBidVolume() << std::endl;
        std::cout << "Total Ask Volume: " << client->GetTotalAskVolume() << std::endl;
        
        std::cout << "\n2. Adding aggressive order that crosses spread..." << std::endl;
        client->SubmitOrder(3, true, 250, 415040); // Buy 250 @ 4150.40
        
        std::cout << "\n=== Updated Order Book Status ===" << std::endl;
        std::cout << "Best Bid: " << std::fixed << std::setprecision(2) << (client->GetBestBid() / 100.0) << std::endl;
        std::cout << "Best Ask: " << std::fixed << std::setprecision(2) << (client->GetBestAsk() / 100.0) << std::endl;
        std::cout << "Total Bid Volume: " << client->GetTotalBidVolume() << std::endl;
        std::cout << "Total Ask Volume: " << client->GetTotalAskVolume() << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error in basic demo: " << e.what() << std::endl;
    }
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
        OrderBookManager manager;
        
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
        
        OrderBookManager manager;
        
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
    std::cout << "\n=== Basic Order Book Demo (No External Data) ===" << std::endl;
    
    // Create a shared order book instance
    auto order_book = std::make_shared<OrderBook>();
    
    // Create a client using the IClient interface
    auto client = std::make_shared<DatabentoMboClient>(1, "Demo Client", order_book);
    
    // Register the client with the OrderBook to receive callbacks
    order_book->RegisterClient(client);
    
    std::cout << "1. Adding initial orders via client interface..." << std::endl;
    
    try {
        // Submit orders through the client interface
        client->SubmitOrder(1, false, 100, 415025); // Sell 100 @ 4150.25
        client->SubmitOrder(1, false, 200, 415050); // Sell 200 @ 4150.50  
        client->SubmitOrder(1, false, 150, 415035); // Sell 150 @ 4150.35
        
        client->SubmitOrder(2, true, 80, 415000);   // Buy 80 @ 4150.00
        client->SubmitOrder(2, true, 120, 414975);  // Buy 120 @ 4149.75
        client->SubmitOrder(2, true, 90, 415010);   // Buy 90 @ 4150.10
        
        std::cout << "\n=== Order Book Status via Client ===" << std::endl;
        std::cout << "Best Bid: " << std::fixed << std::setprecision(2) << (client->GetBestBid() / 100.0) << std::endl;
        std::cout << "Best Ask: " << std::fixed << std::setprecision(2) << (client->GetBestAsk() / 100.0) << std::endl;
        std::cout << "Mid Price: " << std::fixed << std::setprecision(2) << (client->GetMidPrice() / 100.0) << std::endl;
        std::cout << "Spread: " << std::fixed << std::setprecision(2) << (client->GetSpread() / 100.0) << std::endl;
        std::cout << "Total Bid Volume: " << client->GetTotalBidVolume() << std::endl;
        std::cout << "Total Ask Volume: " << client->GetTotalAskVolume() << std::endl;
        
        std::cout << "\n2. Adding aggressive order that crosses spread..." << std::endl;
        client->SubmitOrder(3, true, 250, 415040); // Buy 250 @ 4150.40
        
        std::cout << "\n=== Updated Order Book Status ===" << std::endl;
        client->OnTopOfBookUpdate(client->GetBestBid(), client->GetBestAsk(), 
                                 client->GetTotalBidVolume(), client->GetTotalAskVolume());
        
        // Unregister and shutdown the client
        order_book->UnregisterClient(client->GetClientId());
        
    } catch (const std::exception& e) {
        std::cerr << "Error in basic demo: " << e.what() << std::endl;
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
    
    // Always run the basic demo to show client interface
    //runBasicOrderBookDemo();
    
    // Try to run historical demo if API key is available  
    runHistoricalDataDemo();
    



    return 0;
}
