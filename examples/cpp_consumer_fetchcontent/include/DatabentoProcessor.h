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

#include "IClient.h"

// Include Databento headers
#include <databento/dbn.hpp>
#include <databento/dbn_file_store.hpp>
#include <databento/historical.hpp>
#include <databento/live.hpp>
#include <databento/symbol_map.hpp>

using namespace databento;

/**
 * @brief Databento-specific data processor
 * 
 * This class processes Databento market data and directly updates the OrderBook.
 * It provides proper encapsulation by separating Databento-specific logic from
 * order book management.
 */
class DatabentoProcessor {
private:

    std::shared_ptr<IClient> order_client_;   

    
    
    // Order ID mapping between Databento and internal IDs
    std::unordered_map<uint64_t, uint64_t> databento_to_internal_order_id_;
    std::unordered_map<uint64_t, uint64_t> internal_to_databento_order_id_;
    
    uint64_t last_mbo_timestamp_ = 0;
    std::string current_symbol_;
    uint64_t databento_user_id = 0;
    
public:
    /**
     * @brief Constructor for DatabentoProcessor
     */
    DatabentoProcessor();
    


    void SetOrderClient(std::shared_ptr<IClient> order_client);    
    /**
     * @brief Process incoming Databento market data records
     * @param rec The Databento record to process
     * @return KeepGoing::Continue to continue processing, KeepGoing::Stop to stop
     */
    KeepGoing ProcessMarketData(const Record &rec);
    
    // Accessors for market state
    uint64_t GetLastMboTimestamp() const { return last_mbo_timestamp_; }
    const std::string& GetCurrentSymbol() const { return current_symbol_; }
    bool ClearOrderFromMapping(uint64_t internal_order_id);    
private:
    KeepGoing ProcessMboMessage(const MboMsg &mbo);
    KeepGoing ProcessTradeMessage(const TradeMsg &trade);
    KeepGoing ProcessQuoteMessage(const Mbp1Msg &mbp1);
    void PrintOrderBookStatus();
    
    // Helper methods for order ID mapping
    uint64_t MapDatabentoOrderId(uint64_t databento_order_id, uint64_t internal_order_id);
    uint64_t GetInternalOrderId(uint64_t databento_order_id);
    uint64_t GetDatabentoOrderId(uint64_t internal_order_id);
    
    // Symbol conversion helpers
    std::string ConvertSymbol(uint32_t instrument_id);
};