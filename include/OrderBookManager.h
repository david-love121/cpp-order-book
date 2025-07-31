#pragma once

#include <memory>
#include <atomic>
#include "IClient.h"
#include "OrderBook.h"
#include "IStrategy.h"
#include "OrderBookSnapshot.h"
#include "TopOfBookTracker.h"
#include "PortfolioManager.h"
#include "Trade.h"
#include "Logger.h"
#include "InMemorySink.h"

// Forward declarations
class ParquetProcessor;

/**
 * @brief Central orchestrator for order book operations and market data
 * 
 * OrderBookManager acts as the main IClient implementation and coordinates
 * all order book activities, market data processing, and strategy execution.
 * It owns and orchestrates all components without complex inheritance chains.
 */
class OrderBookManager : public IClient, public std::enable_shared_from_this<OrderBookManager> {
private:
    // Core components (owned and orchestrated)
    std::shared_ptr<OrderBook> order_book_;
    std::shared_ptr<PortfolioManager> portfolio_manager_;
    std::shared_ptr<TopOfBookTracker> tob_tracker_;
    std::shared_ptr<IDataSink> data_sink_;
    std::shared_ptr<IStrategy> strategy_;
    std::shared_ptr<ParquetProcessor> data_processor_;
    
    // Client identification
    uint64_t client_id_;
    std::string client_name_;
    uint64_t tracked_user_id_;
    double max_leverage_ = 1.0;
    double initial_cash_ = 100000.0;
    std::atomic<bool> running_{false};
    
    // Current market state for orchestration
    std::string current_symbol_;
    uint64_t last_timestamp_ = 0;
    
public:
    /**
     * @brief Constructor for OrderBookManager orchestrator
     * @param tracked_user_id User ID to track in portfolio
     * @param max_leverage Maximum leverage allowed
     * @param initial_cash Initial cash balance
     */
    OrderBookManager(uint64_t tracked_user_id = 0, double max_leverage = 1.0, double initial_cash = 100000.0);
    
    // ========== IClient Interface Implementation ==========

    uint64_t GetBestBid() const override;
    uint64_t GetBestAsk() const override;
    uint64_t GetTotalBidVolume() const override;
    uint64_t GetTotalAskVolume() const override;
    uint64_t GetSpread() const override;
    uint64_t GetMidPrice() const override;
    uint64_t SubmitOrder(uint64_t user_id, bool is_buy, uint64_t quantity, uint64_t price,
                                uint64_t ts_received, uint64_t ts_executed) override;
    void CancelOrder(uint64_t order_id) override;
    void ModifyOrder(uint64_t order_id, uint64_t new_quantity, uint64_t new_price, uint64_t new_ts_received, uint64_t new_ts_executed) override;
    // ========== IClient Event Handlers ==========
    
    void OnTradeExecuted(const Trade &trade) override;
    void OnOrderAcknowledged(uint64_t order_id) override;
    void OnOrderCancelled(uint64_t order_id) override;
    void OnOrderModified(uint64_t order_id, uint64_t new_quantity,
                         uint64_t new_price) override;
    void OnOrderRejected(uint64_t order_id, const std::string &reason) override;
    void OnOrderFilled(uint64_t order_id) override;
    void OnTopOfBookUpdate(uint64_t best_bid, uint64_t best_ask,
                           uint64_t bid_volume, uint64_t ask_volume) override;

    void UpdateMarketState(const std::string& symbol, uint64_t timestamp) override;

    void Initialize() override;
    void InitializeAfterConstruction(); // Called after shared_ptr is fully constructed
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
    std::shared_ptr<IDataSink> GetDataSink() { return data_sink_; }
    
    // Strategy management
    void SetStrategy(std::shared_ptr<IStrategy> strategy);
    std::shared_ptr<IStrategy> GetStrategy() const;
    
    // Market data processing
    void RunBacktest(const std::string& data_path);
    
    // Provide IClient interface access for external use
    std::shared_ptr<IClient> GetClient();
    
    // Configuration
    uint64_t GetTrackedUserId() const;
    bool IsUserTracked(uint64_t user_id) const;

    
    // L3 Order Book data access
    OrderBookSnapshot GetOrderBookSnapshot() const;

private:
    // Internal orchestration methods
    void RouteToPortfolio(const Trade& trade);
    void RouteToTopOfBookTracker(uint64_t best_bid, uint64_t best_ask,
                                uint64_t bid_volume, uint64_t ask_volume);
    void NotifyStrategyOfOrderBookChange();
};