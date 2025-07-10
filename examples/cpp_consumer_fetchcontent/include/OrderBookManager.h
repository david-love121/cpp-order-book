#pragma once

#include <memory>
#include "DatabentoMboClient.h"
#include "OrderBook.h"
#include "IClient.h"
#include "IStrategy.h"
#include "TopOfBookTracker.h"

// Include Databento headers
#include <databento/live.hpp>
#include <databento/historical.hpp>
#include <databento/symbol_map.hpp>
#include <databento/dbn.hpp>
#include <databento/dbn_file_store.hpp>

using namespace databento;

// Forward declaration
class OrderBookManager;

/**
 * @brief Manager class to coordinate Databento data with the client
 * 
 * This class serves as a bridge between Databento data feeds and the client,
 * handling the data flow and client lifecycle management.
 */
class OrderBookManagerClient : public DatabentoMboClient {
private:
    OrderBookManager* manager_;
    
public:
    OrderBookManagerClient(uint64_t client_id, const std::string &name,
                          std::shared_ptr<OrderBook> order_book, uint64_t tracked_user_id,
                          uint64_t slippage_delay_ns, OrderBookManager* manager)
        : DatabentoMboClient(client_id, name, order_book, tracked_user_id, slippage_delay_ns), manager_(manager) {}

    // Override virtual callback to forward TOB updates to manager
    void OnTopOfBookCallback(uint64_t best_bid, uint64_t best_ask,
                             uint64_t bid_volume, uint64_t ask_volume) override;

    // Public accessors for protected members
    uint64_t GetPublicLastMboTimestamp() const { return GetLastMboTimestamp(); }
    const std::string& GetPublicCurrentSymbol() const { return GetCurrentSymbol(); }
};

class OrderBookManager {
private:
    std::shared_ptr<OrderBookManagerClient> client_;
    std::shared_ptr<OrderBook> order_book_;
    std::shared_ptr<IStrategy> strategy_;
    
public:
    OrderBookManager(uint64_t slippage_delay_ns = 1000000, uint64_t tracked_user_id = 0);  // Default 1ms slippage, user 0
    
    void Start();
    void Stop();
    
    // Bridge method for Databento callbacks
    KeepGoing OnMarketData(const Record& record);
    
    // Provide access to client for additional operations
    std::shared_ptr<IClient> GetClient();

    // Strategy management
    void SetStrategy(std::shared_ptr<IStrategy> strategy);
    std::shared_ptr<IStrategy> GetStrategy() const;

    // TOB callback handler (called by OrderBookManagerClient)
    void OnTopOfBookUpdate(uint64_t best_bid, uint64_t best_ask,
                          uint64_t bid_volume, uint64_t ask_volume);
};
