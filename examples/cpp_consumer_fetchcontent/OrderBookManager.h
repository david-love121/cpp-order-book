#pragma once

#include <memory>
#include "DatabentoMboClient.h"
#include "OrderBook.h"
#include "IClient.h"

// Include Databento headers
#include <databento/live.hpp>
#include <databento/historical.hpp>
#include <databento/symbol_map.hpp>
#include <databento/dbn.hpp>
#include <databento/dbn_file_store.hpp>

using namespace databento;

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
    OrderBookManager(uint64_t slippage_delay_ns = 1000000);  // Default 1ms slippage
    
    void Start();
    void Stop();
    
    // Bridge method for Databento callbacks
    KeepGoing OnMarketData(const Record& record);
    
    // Provide access to client for additional operations
    std::shared_ptr<IClient> GetClient();
};
