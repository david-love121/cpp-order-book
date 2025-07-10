#pragma once

#include <memory>
#include <atomic>
#include "IClient.h"
#include "OrderBook.h"
#include "IStrategy.h"
#include "TopOfBookTracker.h"
#include "PortfolioManager.h"

// Include Databento headers for market data processing
#include <databento/live.hpp>
#include <databento/historical.hpp>
#include <databento/symbol_map.hpp>
#include <databento/dbn.hpp>
#include <databento/dbn_file_store.hpp>

using namespace databento;

// Forward declarations
class DatabentoProcessor;

/**
 * @brief Central orchestrator for order book operations and market data
 * 
 * OrderBookManager acts as the main IClient implementation and coordinates
 * all order book activities, market data processing, and strategy execution.
 * It owns and orchestrates all components without complex inheritance chains.
 */
class OrderBookManager : public IClient {
private:
    // Core components (owned and orchestrated)
    std::shared_ptr<OrderBook> order_book_;
    std::shared_ptr<PortfolioManager> portfolio_manager_;
    std::shared_ptr<TopOfBookTracker> tob_tracker_;
    std::shared_ptr<IStrategy> strategy_;
    std::shared_ptr<DatabentoProcessor> databento_processor_;
    
    // Client identification
    uint64_t client_id_;
    std::string client_name_;
    uint64_t tracked_user_id_;
    uint64_t slippage_delay_ns_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> next_order_id_{1000};
    
    // Current market state for orchestration
    std::string current_symbol_;
    uint64_t last_timestamp_ = 0;
    
public:
    /**
     * @brief Constructor for OrderBookManager orchestrator
     * @param slippage_delay_ns Slippage delay for order execution simulation
     * @param tracked_user_id User ID to track in portfolio
     */
    OrderBookManager(uint64_t slippage_delay_ns = 1000000, uint64_t tracked_user_id = 0);
    
    // ========== IClient Interface Implementation ==========
    
    uint64_t SubmitOrder(uint64_t user_id, bool is_buy, uint64_t quantity,
                         uint64_t price, uint64_t ts_received,
                         uint64_t ts_executed) override;
    uint64_t SubmitOrder(uint64_t user_id, bool is_buy, uint64_t quantity,
                         uint64_t price) override;
    bool CancelOrder(uint64_t order_id) override;
    bool ModifyOrder(uint64_t order_id, uint64_t new_quantity,
                     uint64_t new_price) override;
    uint64_t GetBestBid() const override;
    uint64_t GetBestAsk() const override;
    uint64_t GetTotalBidVolume() const override;
    uint64_t GetTotalAskVolume() const override;
    uint64_t GetSpread() const override;
    uint64_t GetMidPrice() const override;

    // ========== IClient Event Handlers ==========
    
    void OnTradeExecuted(const Trade &trade) override;
    void OnOrderAcknowledged(uint64_t order_id) override;
    void OnOrderCancelled(uint64_t order_id) override;
    void OnOrderModified(uint64_t order_id, uint64_t new_quantity,
                         uint64_t new_price) override;
    void OnOrderRejected(uint64_t order_id, const std::string &reason) override;
    void OnTopOfBookUpdate(uint64_t best_bid, uint64_t best_ask,
                           uint64_t bid_volume, uint64_t ask_volume) override;

    void Initialize() override;
    void Shutdown() override;
    uint64_t GetClientId() const override;
    std::string GetClientName() const override;

    // ========== Orchestrator Management Methods ==========
    
    void Start();
    void Stop();
    
    // Component access (for external configuration)
    std::shared_ptr<PortfolioManager> GetPortfolioManager() const;
    std::shared_ptr<TopOfBookTracker> GetTopOfBookTracker() const;
    std::shared_ptr<OrderBook> GetOrderBook() const;
    
    // Strategy management
    void SetStrategy(std::shared_ptr<IStrategy> strategy);
    std::shared_ptr<IStrategy> GetStrategy() const;
    
    // Market data processing
    KeepGoing ProcessMarketData(const Record& record);
    
    // Provide IClient interface access for external use
    std::shared_ptr<IClient> GetClient();
    
    // Configuration
    uint64_t GetTrackedUserId() const;
    bool IsUserTracked(uint64_t user_id) const;
    void SetSlippageDelay(uint64_t slippage_delay_ns);
    uint64_t GetSlippageDelay() const;
    
    // Market state management (for DatabentoProcessor)
    void UpdateMarketState(const std::string& symbol, uint64_t timestamp);

private:
    // Internal orchestration methods
    uint64_t GenerateOrderId();
    void RouteToPortfolio(const Trade& trade);
    void RouteToStrategy(uint64_t best_bid, uint64_t best_ask, 
                        uint64_t bid_volume, uint64_t ask_volume);
    void RouteToTopOfBookTracker(uint64_t best_bid, uint64_t best_ask,
                                uint64_t bid_volume, uint64_t ask_volume);
};