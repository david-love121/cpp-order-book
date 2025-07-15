#pragma once

#include "IStrategy.h"
#include <deque>
#include <memory>

class PortfolioManager;
class IClient;

/**
 * @brief High-frequency order book imbalance strategy
 * 
 * This strategy identifies trading opportunities based on order book imbalances
 * between bid and ask volumes. It uses a rolling window to track imbalance
 * patterns and generates signals when significant imbalances are detected.
 * 
 * Key features:
 * - Rolling window imbalance tracking
 * - Momentum-based signal generation
 * - Configurable thresholds and lookback periods
 * - High-frequency trading suitable
 */
class OrderImbalanceStrategy : public Strategy {
private:
    // Strategy parameters
    double imbalance_threshold_;     // Minimum imbalance to generate signal
    double momentum_factor_;         // Amplification factor for momentum
    double decay_factor_;           // Decay factor for historical signals
    size_t lookback_window_;        // Number of snapshots to consider

    // Historical data tracking
    std::deque<double> imbalance_history_;      // Rolling window of imbalances
    std::deque<double> price_history_;          // Rolling window of mid prices
    std::deque<uint64_t> volume_history_;       // Rolling window of total volumes
    
    // Signal state
    double current_signal_;         // Current signal strength
    double signal_momentum_;        // Signal momentum factor

        
    // Risk management
    double max_signal_strength_;    // Maximum signal strength cap
    // Auto-trading functionality
    std::shared_ptr<IClient> order_client_;  // Client for placing orders
    bool auto_trading_enabled_;     // Enable/disable automatic order placement
    uint64_t last_order_time_;      // Timestamp of last order placed
    double min_signal_for_trade_;   // Minimum signal strength to place order

    std::deque<uint64_t> recent_order_times_; // Track recent order timestamps

public:
    /**
     * @brief Constructor for OrderImbalanceStrategy
     * @param name Strategy name
     * @param user_id User ID for this strategy
     * @param imbalance_threshold Minimum imbalance to act on (default: 0.15)
     * @param lookback_window Number of snapshots to track (default: 20)
     */
    OrderImbalanceStrategy(const std::string& name, 
                          uint64_t user_id,
                          double imbalance_threshold = 0.15,
                          size_t lookback_window = 20);


    /**
     * @brief Process full order book data and generate trading signals
     * @param orderbook_snapshot Full order book snapshot with L3 data
     * @return Strategy action based on L3 order imbalance
     */
    StrategyAction ProcessOrderBookData(const OrderBookSnapshot& orderbook_snapshot) override;

    /**
     * @brief Reset strategy state and clear history
     */
    void Reset() override;


    // Getters for strategy metrics
    double GetCurrentSignal() const { return current_signal_; }
    double GetSignalMomentum() const { return signal_momentum_; }
    size_t GetHistorySize() const { return imbalance_history_.size(); }
    
    // Configuration methods
    void SetImbalanceThreshold(double threshold) { imbalance_threshold_ = threshold; }
    void SetMomentumFactor(double factor) { momentum_factor_ = factor; }
    void SetDecayFactor(double factor) { decay_factor_ = factor; }
    void SetLookbackWindow(size_t window);

    // Auto-trading configuration
    void SetOrderClient(std::shared_ptr<IClient> client) { order_client_ = client; }
    void EnableAutoTrading(bool enabled = true) { auto_trading_enabled_ = enabled; }
    void SetMinSignalForTrade(double signal) { min_signal_for_trade_ = signal; }


    
    // Auto-trading status
    bool IsAutoTradingEnabled() const { return auto_trading_enabled_; }
    uint64_t GetOrderCount() const { return recent_order_times_.size(); }

private:

    /**
     * @brief Calculate L3 order imbalance from full order book
     * @param orderbook_snapshot Full order book snapshot
     * @return Imbalance value [-1.0, 1.0]
     */
    double CalculateL3Imbalance(const OrderBookSnapshot& orderbook_snapshot) const;

    /**
     * @brief Calculate momentum-adjusted signal
     * @param raw_imbalance Raw imbalance value
     * @return Momentum-adjusted signal
     */
    double CalculateMomentumSignal(double raw_imbalance);


    /**
     * @brief Update historical data with L3 order book snapshot
     * @param orderbook_snapshot Full order book snapshot
     */
    void UpdateHistoryFromOrderBook(const OrderBookSnapshot& orderbook_snapshot);

    /**
     * @brief Calculate signal strength based on imbalance pattern
     * @param imbalance Current imbalance value
     * @return Signal strength [0.0, 1.0]
     */
    double CalculateSignalStrength(double imbalance) const;


    /**
     * @brief Check if current market conditions support trading
     * @param orderbook_snapshot Order book snapshot
     * @return True if conditions are suitable for trading
     */
    bool IsMarketConditionsSuitable(const OrderBookSnapshot& orderbook_snapshot) const;
    
    /**
     * @brief Execute auto-trading based on signal
     * @param signal Signal strength [-1.0, 1.0]
     * @param orderbook_snapshot Current order book snapshot
     * @return True if order was placed
     */
    bool ExecuteAutoTrade(double signal, const OrderBookSnapshot& orderbook_snapshot);
    
    
    /**
     * @brief Calculate order price based on signal and market conditions
     * @param signal Signal strength
     * @param orderbook_snapshot Order book snapshot
     * @param is_buy Whether this is a buy order
     * @return Order price in ticks
     */
    uint64_t CalculateOrderPrice(double signal, const OrderBookSnapshot& orderbook_snapshot, bool is_buy) const;
    
    /**
     * @brief Calculate order quantity based on signal strength
     * @param signal Signal strength
     * @return Order quantity
     */
    uint64_t CalculateOrderQuantity(double signal) const;
    
    /**
     * @brief Calculate weighted volume imbalance across multiple price levels
     * @param orderbook_snapshot Full order book snapshot
     * @param depth Number of price levels to consider
     * @return Weighted imbalance value
     */
    double CalculateWeightedImbalance(const OrderBookSnapshot& orderbook_snapshot, size_t depth = 5) const;
    
    /**
     * @brief Calculate order count imbalance (number of orders, not volume)
     * @param orderbook_snapshot Full order book snapshot
     * @return Order count imbalance
     */
    double CalculateOrderCountImbalance(const OrderBookSnapshot& orderbook_snapshot) const;
    
    /**
     * @brief Calculate price level density analysis
     * @param orderbook_snapshot Full order book snapshot
     * @return Density-based signal
     */
    double CalculatePriceLevelDensity(const OrderBookSnapshot& orderbook_snapshot) const;
};