#pragma once

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
#include "PortfolioManager.h"
#include "TopOfBookTracker.h"

// Include Databento headers
#include <databento/live.hpp>
#include <databento/historical.hpp>
#include <databento/symbol_map.hpp>
#include <databento/dbn.hpp>
#include <databento/dbn_file_store.hpp>

using namespace databento;

/**
 * @brief MBO (Market By Order) client implementation for Databento integration
 * 
 * This client handles real-time and historical Databento MBO data streams,
 * processes order book updates, and maintains portfolio state.
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
    
    // Portfolio manager for trade tracking and CSV export
    std::shared_ptr<PortfolioManager> portfolio_manager_;
    
    // Top of book tracker for TOB CSV export
    std::shared_ptr<TopOfBookTracker> tob_tracker_;
    
    std::string current_symbol_;
    uint64_t tracked_user_id_;

public:
    DatabentoMboClient(uint64_t client_id, const std::string& name, std::shared_ptr<OrderBook> order_book, 
                      uint64_t tracked_user_id, uint64_t slippage_delay_ns = 0);  // slippage_delay_ns ignored

    // ========== IClient Interface Implementation ==========
    
    uint64_t SubmitOrder(uint64_t user_id, bool is_buy, uint64_t quantity, uint64_t price, 
                        uint64_t ts_received, uint64_t ts_executed) override;
    uint64_t SubmitOrder(uint64_t user_id, bool is_buy, uint64_t quantity, uint64_t price) override;
    bool CancelOrder(uint64_t order_id) override;
    bool ModifyOrder(uint64_t order_id, uint64_t new_quantity, uint64_t new_price) override;
    uint64_t GetBestBid() const override;
    uint64_t GetBestAsk() const override;
    uint64_t GetTotalBidVolume() const override;
    uint64_t GetTotalAskVolume() const override;
    uint64_t GetSpread() const override;
    uint64_t GetMidPrice() const override;

    // ========== IClient Event Handlers ==========

    void OnTradeExecuted(const Trade& trade) override;
    void OnOrderAcknowledged(uint64_t order_id) override;
    void OnOrderCancelled(uint64_t order_id) override;
    void OnOrderModified(uint64_t order_id, uint64_t new_quantity, uint64_t new_price) override;
    void OnOrderRejected(uint64_t order_id, const std::string& reason) override;
    void OnTopOfBookUpdate(uint64_t best_bid, uint64_t best_ask, uint64_t bid_volume, uint64_t ask_volume) override;

    void Initialize() override;
    void Shutdown() override;
    uint64_t GetClientId() const override;
    std::string GetClientName() const override;
    
    // Portfolio management access
    std::shared_ptr<PortfolioManager> GetPortfolioManager() const;
    
    // Top of book tracker access
    std::shared_ptr<TopOfBookTracker> GetTopOfBookTracker() const;
    
    /**
     * @brief Get the tracked user ID
     * @return The user ID being tracked by this client
     */
    uint64_t GetTrackedUserId() const;
    
    /**
     * @brief Check if a user ID is the tracked user
     * @param user_id User ID to check
     * @return true if this is the tracked user, false otherwise
     */
    bool IsUserTracked(uint64_t user_id) const;

    /**
     * @brief Set the slippage delay for portfolio timestamp simulation
     * @param slippage_delay_ns Delay in nanoseconds to add to trade timestamps
     */
    void SetSlippageDelay(uint64_t slippage_delay_ns);
    
    /**
     * @brief Get the current slippage delay
     * @return Current slippage delay in nanoseconds  
     */
    uint64_t GetSlippageDelay() const;

    // ========== Databento MBO Message Processing ==========

    /**
     * @brief Process incoming Databento market data records
     * @param rec The Databento record to process
     * @return KeepGoing::Continue to continue processing, KeepGoing::Stop to stop
     */
    KeepGoing ProcessMarketData(const Record& rec);

private:
    uint64_t GenerateOrderId();
    KeepGoing ProcessMboMessage(const MboMsg& mbo);
    KeepGoing ProcessTradeMessage(const TradeMsg& trade);
    KeepGoing ProcessQuoteMessage(const Mbp1Msg& mbp1);
    void PrintOrderBookStatus();
    void printOrderBookStatus(); // Alias for compatibility
};
