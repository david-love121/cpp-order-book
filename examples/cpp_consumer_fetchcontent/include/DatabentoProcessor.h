#pragma once

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

// Include the OrderBook headers
#include "Order.h"
#include "OrderBook.h"
#include "Trade.h"

// Include Databento headers
#include <databento/dbn.hpp>
#include <databento/dbn_file_store.hpp>
#include <databento/historical.hpp>
#include <databento/live.hpp>
#include <databento/symbol_map.hpp>

using namespace databento;

// Forward declaration
class OrderBookManager;

/**
 * @brief Pure processor for Databento market data
 * 
 * This class handles the parsing and processing of Databento MBO data
 * without inheriting from IClient. It's a pure component that processes
 * market data and reports results back to the OrderBookManager orchestrator.
 */
class DatabentoProcessor {
private:
    OrderBookManager* manager_;  // Back-reference to orchestrator
    std::shared_ptr<OrderBook> order_book_;
    
    // Symbol mapping for Databento instrument IDs
    PitSymbolMap symbol_mappings_;
    
    // Track last prices for market making
    std::unordered_map<std::string, uint64_t> last_price_by_symbol_;
    
    // Order ID mapping between Databento and internal IDs
    std::unordered_map<uint64_t, uint64_t> databento_to_internal_order_id_;
    std::unordered_map<uint64_t, uint64_t> internal_to_databento_order_id_;
    
    uint64_t last_mbo_timestamp_ = 0;
    
public:
    /**
     * @brief Constructor for DatabentoProcessor
     * @param manager Back-reference to the orchestrating OrderBookManager
     * @param order_book Order book to process data into
     */
    DatabentoProcessor(OrderBookManager* manager, std::shared_ptr<OrderBook> order_book);
    
    /**
     * @brief Process incoming Databento market data records
     * @param rec The Databento record to process
     * @return KeepGoing::Continue to continue processing, KeepGoing::Stop to stop
     */
    KeepGoing ProcessMarketData(const Record &rec);
    
    // Accessors for market state
    uint64_t GetLastMboTimestamp() const { return last_mbo_timestamp_; }
    
private:
    KeepGoing ProcessMboMessage(const MboMsg &mbo);
    KeepGoing ProcessTradeMessage(const TradeMsg &trade);
    KeepGoing ProcessQuoteMessage(const Mbp1Msg &mbp1);
    void PrintOrderBookStatus();
    
    // Helper methods for order ID mapping
    uint64_t MapDatabentoOrderId(uint64_t databento_order_id, uint64_t internal_order_id);
    uint64_t GetInternalOrderId(uint64_t databento_order_id);
};