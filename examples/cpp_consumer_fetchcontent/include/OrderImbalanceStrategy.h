#pragma once

#include "IStrategy.h"
#include <deque>
#include <memory>

class PortfolioManager;
class DatabentoMboClient;

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
    uint64_t last_signal_time_;     // Timestamp of last signal
    
    // Risk management
    double max_signal_strength_;    // Maximum signal strength cap
    double position_limit_factor_;  // Position limit as factor of base quantity
    
    // Auto-trading functionality
    std::shared_ptr<DatabentoMboClient> order_client_;  // Client for placing orders
    bool auto_trading_enabled_;     // Enable/disable automatic order placement
    uint64_t last_order_time_;      // Timestamp of last order placed
    uint64_t min_order_interval_;   // Minimum time between orders (nanoseconds)
    double min_signal_for_trade_;   // Minimum signal strength to place order
    uint64_t max_orders_per_minute_;// Maximum orders per minute rate limit
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
     * @brief Process market data and generate trading signals
     * @param snapshot Market data snapshot
     * @return Strategy action based on order imbalance
     */
    StrategyAction ProcessMarketData(const MarketSnapshot& snapshot) override;

    /**
     * @brief Reset strategy state and clear history
     */
    void Reset() override;

    /**
     * @brief Initialize strategy with custom parameters
     * @param parameters Strategy parameters map
     */
    void Initialize(const std::unordered_map<std::string, double>& parameters) override;

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
    void SetOrderClient(std::shared_ptr<DatabentoMboClient> client) { order_client_ = client; }
    void EnableAutoTrading(bool enabled = true) { auto_trading_enabled_ = enabled; }
    void SetMinSignalForTrade(double signal) { min_signal_for_trade_ = signal; }
    void SetMinOrderInterval(uint64_t interval_ns) { min_order_interval_ = interval_ns; }
    void SetMaxOrdersPerMinute(uint64_t max_orders) { max_orders_per_minute_ = max_orders; }
    
    // Auto-trading status
    bool IsAutoTradingEnabled() const { return auto_trading_enabled_; }
    uint64_t GetOrderCount() const { return recent_order_times_.size(); }

private:
    /**
     * @brief Calculate order imbalance from market snapshot
     * @param snapshot Market data snapshot
     * @return Imbalance value [-1.0, 1.0]
     */
    double CalculateImbalance(const MarketSnapshot& snapshot) const;

    /**
     * @brief Calculate momentum-adjusted signal
     * @param raw_imbalance Raw imbalance value
     * @return Momentum-adjusted signal
     */
    double CalculateMomentumSignal(double raw_imbalance);

    /**
     * @brief Update historical data with new snapshot
     * @param snapshot Market data snapshot
     */
    void UpdateHistory(const MarketSnapshot& snapshot);

    /**
     * @brief Calculate signal strength based on imbalance pattern
     * @param imbalance Current imbalance value
     * @return Signal strength [0.0, 1.0]
     */
    double CalculateSignalStrength(double imbalance) const;

    /**
     * @brief Apply risk management rules to signal
     * @param signal Raw signal value
     * @return Risk-adjusted signal
     */
    double ApplyRiskManagement(double signal) const;

    /**
     * @brief Check if current market conditions support trading
     * @param snapshot Market data snapshot
     * @return True if conditions are suitable for trading
     */
    bool IsMarketConditionsSuitable(const MarketSnapshot& snapshot) const;
    
    /**
     * @brief Execute auto-trading based on signal
     * @param signal Signal strength [-1.0, 1.0]
     * @param snapshot Current market snapshot
     * @return True if order was placed
     */
    bool ExecuteAutoTrade(double signal, const MarketSnapshot& snapshot);
    
    /**
     * @brief Check if order placement is allowed (rate limiting)
     * @param current_time Current timestamp
     * @return True if order can be placed
     */
    bool CanPlaceOrder(uint64_t current_time);
    
    /**
     * @brief Calculate order price based on signal and market conditions
     * @param signal Signal strength
     * @param snapshot Market snapshot
     * @param is_buy Whether this is a buy order
     * @return Order price in ticks
     */
    uint64_t CalculateOrderPrice(double signal, const MarketSnapshot& snapshot, bool is_buy) const;
    
    /**
     * @brief Calculate order quantity based on signal strength
     * @param signal Signal strength
     * @return Order quantity
     */
    uint64_t CalculateOrderQuantity(double signal) const;
};