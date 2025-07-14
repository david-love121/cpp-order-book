#include "OrderImbalanceStrategy.h"
#include "PortfolioManager.h"
#include "IClient.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>

OrderImbalanceStrategy::OrderImbalanceStrategy(const std::string& name, 
                                              uint64_t user_id,
                                              double imbalance_threshold,
                                              size_t lookback_window)
    : Strategy(name, user_id)
    , imbalance_threshold_(imbalance_threshold)
    , momentum_factor_(1.5)
    , decay_factor_(0.95)
    , lookback_window_(lookback_window)
    , current_signal_(0.0)
    , signal_momentum_(0.0)
    , last_signal_time_(0)
    , max_signal_strength_(1.0)
    , position_limit_factor_(2.0)
    , order_client_(nullptr)
    , auto_trading_enabled_(false)
    , last_order_time_(0)
    , min_order_interval_(1000000000)  // 1 second minimum between orders
    , min_signal_for_trade_(0.15)      // 15% minimum signal to trade
    , max_orders_per_minute_(30) {     // Max 30 orders per minute
    
    // Initialize strategy-specific parameters
    SetParameter("imbalance_threshold", imbalance_threshold_);
    SetParameter("momentum_factor", momentum_factor_);
    SetParameter("decay_factor", decay_factor_);
    SetParameter("lookback_window", static_cast<double>(lookback_window_));
    SetParameter("max_signal_strength", max_signal_strength_);
    SetParameter("position_limit_factor", position_limit_factor_);
    SetParameter("auto_trading_enabled", auto_trading_enabled_ ? 1.0 : 0.0);
    SetParameter("min_signal_for_trade", min_signal_for_trade_);
    SetParameter("min_order_interval_ms", min_order_interval_ / 1000000.0);  // Convert to ms
    SetParameter("max_orders_per_minute", static_cast<double>(max_orders_per_minute_));
    
    // Set reasonable defaults for high-frequency trading
    SetSignalThreshold(0.05);  // Lower threshold for HFT
    SetBaseQuantity(10);       // Moderate base quantity
    
    std::cout << "[STRATEGY] OrderImbalanceStrategy '" << name 
              << "' initialized for user " << user_id 
              << " with threshold=" << imbalance_threshold_
              << ", window=" << lookback_window_ << std::endl;
}


StrategyAction OrderImbalanceStrategy::ProcessOrderBookData(const OrderBookSnapshot& orderbook_snapshot) {
    static uint64_t total_calls = 0;
    total_calls++;
    
    if (total_calls <= 5 || total_calls % 1000 == 0) {
        std::cout << "[STRATEGY-L3] ProcessOrderBookData called #" << total_calls 
                  << " for symbol=" << orderbook_snapshot.symbol 
                  << ", enabled=" << IsEnabled() << std::endl;
    }
    
    if (!IsEnabled()) {
        return StrategyAction(StrategySignal::NONE, 0, 0.0);
    }
    
    // Check if we have meaningful order book data
    if (orderbook_snapshot.bid_levels.empty() || orderbook_snapshot.ask_levels.empty()) {
        return StrategyAction(StrategySignal::NONE, 0, 0.0);
    }
    
    // Calculate L3 imbalance using full order book
    double l3_imbalance = CalculateL3Imbalance(orderbook_snapshot);
    
    // Calculate weighted imbalance across multiple levels
    double weighted_imbalance = CalculateWeightedImbalance(orderbook_snapshot, 5);
    
    // Calculate order count imbalance
    double order_count_imbalance = CalculateOrderCountImbalance(orderbook_snapshot);
    
    // Calculate price level density
    double density_signal = CalculatePriceLevelDensity(orderbook_snapshot);
    
    // Combine multiple L3 signals with different weights
    double combined_signal = 0.4 * l3_imbalance + 
                           0.3 * weighted_imbalance + 
                           0.2 * order_count_imbalance + 
                           0.1 * density_signal;
    
    // Update historical data using order book snapshot
    UpdateHistoryFromOrderBook(orderbook_snapshot);
    
    // Calculate momentum-adjusted signal
    double momentum_signal = CalculateMomentumSignal(combined_signal);
    
    // Calculate signal strength
    double signal_strength = CalculateSignalStrength(combined_signal);
    
    // Apply risk management
    double final_signal = ApplyRiskManagement(momentum_signal);
    
    // Update current signal state
    current_signal_ = final_signal;
    last_signal_time_ = orderbook_snapshot.timestamp;
    
    // Execute auto-trading if enabled and signal is strong enough
    bool order_placed = false;
    if (auto_trading_enabled_ && std::abs(final_signal) >= min_signal_for_trade_) {
        order_placed = ExecuteAutoTrade(final_signal, slippage_delay_ns_, orderbook_snapshot);
    }
    
    // Convert signal to action
    StrategyAction action = SignalToAction(final_signal);
    action.confidence = signal_strength;
    
    // Log significant L3 signals
    if (std::abs(final_signal) > GetSignalThreshold()) {
        std::cout << "[STRATEGY-L3] L3 signal=" << final_signal 
                  << ", l3_imbalance=" << l3_imbalance
                  << ", weighted=" << weighted_imbalance
                  << ", order_count=" << order_count_imbalance
                  << ", density=" << density_signal
                  << ", levels=" << orderbook_snapshot.bid_levels.size() + orderbook_snapshot.ask_levels.size()
                  << ", action=" << (action.signal == StrategySignal::BUY ? "BUY" : 
                                   action.signal == StrategySignal::SELL ? "SELL" : "HOLD")
                  << ", qty=" << action.quantity
                  << ", auto_trade=" << (order_placed ? "YES" : "NO") << std::endl;
    }
    
    return action;
}

void OrderImbalanceStrategy::Reset() {
    // Call base class reset
    Strategy::Reset();
    
    // Reset strategy-specific state
    imbalance_history_.clear();
    price_history_.clear();
    volume_history_.clear();
    current_signal_ = 0.0;
    signal_momentum_ = 0.0;
    last_signal_time_ = 0;
    
    // Reset auto-trading state
    last_order_time_ = 0;
    recent_order_times_.clear();
    
    std::cout << "[STRATEGY] OrderImbalanceStrategy reset" << std::endl;
}
//This is slightly clunky, intended to allow setting parameters from config file
void OrderImbalanceStrategy::Initialize(const std::unordered_map<std::string, double>& parameters) {
    // Call base class initialize
    Strategy::Initialize(parameters);
    
    // Update strategy-specific parameters
    imbalance_threshold_ = GetParameter("imbalance_threshold", 0.15);
    momentum_factor_ = GetParameter("momentum_factor", 1.5);
    decay_factor_ = GetParameter("decay_factor", 0.95);
    lookback_window_ = static_cast<size_t>(GetParameter("lookback_window", 20.0));
    max_signal_strength_ = GetParameter("max_signal_strength", 1.0);
    position_limit_factor_ = GetParameter("position_limit_factor", 2.0);
    auto_trading_enabled_ = GetParameter("auto_trading_enabled", 0.0) > 0.5;
    min_signal_for_trade_ = GetParameter("min_signal_for_trade", 0.15);
    min_order_interval_ = static_cast<uint64_t>(GetParameter("min_order_interval_ms", 1000.0) * 1000000.0);  // Convert ms to ns
    max_orders_per_minute_ = static_cast<uint64_t>(GetParameter("max_orders_per_minute", 30.0));
    slippage_delay_ns_ = static_cast<uint64_t>(GetParameter("slippage_delay_ns", 1000000.0)); // Default 1ms
    std::cout << "[STRATEGY] OrderImbalanceStrategy initialized with custom parameters" << std::endl;
}

void OrderImbalanceStrategy::SetLookbackWindow(size_t window) {
    lookback_window_ = window;
    SetParameter("lookback_window", static_cast<double>(window));
    
    // Trim history if new window is smaller
    while (imbalance_history_.size() > lookback_window_) {
        imbalance_history_.pop_front();
    }
    while (price_history_.size() > lookback_window_) {
        price_history_.pop_front();
    }
    while (volume_history_.size() > lookback_window_) {
        volume_history_.pop_front();
    }
}


double OrderImbalanceStrategy::CalculateMomentumSignal(double raw_imbalance) {
    // Update momentum with exponential decay
    signal_momentum_ = signal_momentum_ * decay_factor_ + raw_imbalance * (1.0 - decay_factor_);
    
    // Calculate momentum-enhanced signal
    double momentum_enhanced = raw_imbalance + (signal_momentum_ * momentum_factor_);
    
    // Clamp to reasonable bounds
    return std::max(-max_signal_strength_, std::min(max_signal_strength_, momentum_enhanced));
}


double OrderImbalanceStrategy::CalculateSignalStrength(double imbalance) const {
    if (imbalance_history_.size() < 3) {
        return std::abs(imbalance);  // Not enough history, use raw imbalance
    }
    
    // Calculate rolling statistics
    double sum = std::accumulate(imbalance_history_.begin(), imbalance_history_.end(), 0.0);
    double mean = sum / imbalance_history_.size();
    
    // Calculate standard deviation
    double sq_sum = 0.0;
    for (double val : imbalance_history_) {
        sq_sum += (val - mean) * (val - mean);
    }
    double std_dev = std::sqrt(sq_sum / imbalance_history_.size());
    
    // Signal strength based on how many standard deviations away from mean
    if (std_dev > 0.0) {
        double z_score = std::abs(imbalance - mean) / std_dev;
        return std::min(1.0, z_score / 2.0);  // Normalize to [0.0, 1.0]
    }
    
    return std::abs(imbalance);
}

double OrderImbalanceStrategy::ApplyRiskManagement(double signal) const {
    // Check portfolio manager for position limits
    if (auto portfolio_mgr = GetPortfolioManager()) {
        int64_t current_position = portfolio_mgr->GetRunningPosition();
        double position_limit = GetBaseQuantity() * position_limit_factor_;
        
        // Apply position limits
        if (signal > 0.0 && current_position >= static_cast<int64_t>(position_limit)) {
            return 0.0;  // Already at long position limit
        }
        if (signal < 0.0 && current_position <= -static_cast<int64_t>(position_limit)) {
            return 0.0;  // Already at short position limit
        }
        
        // Scale signal based on current position
        double position_factor = 1.0 - (std::abs(current_position) / position_limit);
        signal *= std::max(0.1, position_factor);  // Minimum 10% signal strength
    }
    
    // Apply signal threshold
    if (std::abs(signal) < imbalance_threshold_) {
        return 0.0;
    }
    
    // Cap signal strength
    return std::max(-max_signal_strength_, std::min(max_signal_strength_, signal));
}

bool OrderImbalanceStrategy::IsMarketConditionsSuitable(const OrderBookSnapshot& orderbook_snapshot) const {
    // Check for valid prices
    if (orderbook_snapshot.GetBestBid() <= 0.0 || orderbook_snapshot.GetBestAsk() <= 0.0) {
        return false;
    }
    
    // Check for reasonable spread
    double spread = orderbook_snapshot.GetSpread();
    double mid_price = orderbook_snapshot.GetMidPrice();
    if (spread <= 0.0 || spread > mid_price * 0.01) {  // Max 1% spread
        return false;
    }
    
    // Check for minimum volume
    if (orderbook_snapshot.GetBestBidVolume() == 0 && orderbook_snapshot.GetBestAskVolume() == 0) {
        return false;
    }
    
    // Check for crossed market
    if (orderbook_snapshot.GetBestBid() >= orderbook_snapshot.GetBestAsk()) {
        return false;
    }
    
    return true;
}

// Auto-trading implementation


bool OrderImbalanceStrategy::ExecuteAutoTrade(double signal, double slippage_delay_ns, const OrderBookSnapshot& orderbook_snapshot) {
    if (!order_client_ || !auto_trading_enabled_) {
        return false;
    }
    
    // Check rate limiting
    if (!CanPlaceOrder(orderbook_snapshot.timestamp)) {
        return false;
    }
    
    // Determine order direction
    bool is_buy = signal > 0.0;
    
    // Calculate order parameters
    uint64_t quantity = CalculateOrderQuantity(std::abs(signal));
    uint64_t price = CalculateOrderPrice(signal, orderbook_snapshot, is_buy);
    
    if (quantity == 0 || price == 0) {
        return false;
    }
    
    // Place the order
    uint64_t synthetic_databento_id = 0;  // Use 0 for strategy-generated orders
    uint64_t order_id = order_client_->SubmitOrder(synthetic_databento_id, GetUserId(), is_buy, quantity, price, orderbook_snapshot.timestamp, orderbook_snapshot.timestamp + slippage_delay_ns_);

    if (order_id > 0) {
        // Record successful order placement
        recent_order_times_.push_back(orderbook_snapshot.timestamp);
        last_order_time_ = orderbook_snapshot.timestamp;
        
        // Clean up old timestamps (keep only last minute)
        uint64_t one_minute_ago = orderbook_snapshot.timestamp - 60000000000ULL; // 60 seconds in nanoseconds
        while (!recent_order_times_.empty() && recent_order_times_.front() < one_minute_ago) {
            recent_order_times_.pop_front();
        }
        
        std::cout << "[AUTO-TRADE] Placed " << (is_buy ? "BUY" : "SELL") 
                  << " order " << order_id << ": " << quantity << " @ " 
                  << (price / 100.0) << " (signal=" << signal << ")" << std::endl;
        
        return true;
    }
    
    return false;
}

bool OrderImbalanceStrategy::CanPlaceOrder(uint64_t current_time) {
    // Check minimum time interval
    if (last_order_time_ > 0 && (current_time - last_order_time_) < min_order_interval_) {
        return false;
    }
    
    // Check rate limiting (orders per minute)
    uint64_t one_minute_ago = current_time - 60000000000ULL; // 60 seconds in nanoseconds
    
    // Count recent orders
    uint64_t recent_orders = 0;
    for (uint64_t order_time : recent_order_times_) {
        if (order_time >= one_minute_ago) {
            recent_orders++;
        }
    }
    
    return recent_orders < max_orders_per_minute_;
}

uint64_t OrderImbalanceStrategy::CalculateOrderPrice(double signal, const OrderBookSnapshot& orderbook_snapshot, bool is_buy) const {
    if (orderbook_snapshot.GetBestBid() <= 0.0 || orderbook_snapshot.GetBestAsk() <= 0.0) {
        return 0;
    }
    
    // Convert prices to ticks (assuming 0.01 tick size, multiply by 100)
    uint64_t bid_ticks = static_cast<uint64_t>(orderbook_snapshot.GetBestBid() * 100.0);
    uint64_t ask_ticks = static_cast<uint64_t>(orderbook_snapshot.GetBestAsk() * 100.0);
    
    // Signal strength determines aggressiveness
    double signal_strength = std::abs(signal);
    
    if (is_buy) {
        // For buy orders: start at bid, move toward ask based on signal strength
        // Strong signals (>0.5) will cross the spread
        if (signal_strength > 0.5) {
            // Aggressive - hit the ask
            return ask_ticks;
        } else if (signal_strength > 0.3) {
            // Semi-aggressive - between bid and ask
            return bid_ticks + static_cast<uint64_t>((ask_ticks - bid_ticks) * signal_strength);
        } else {
            // Passive - at or slightly above bid
            return bid_ticks + 1; // One tick above bid
        }
    } else {
        // For sell orders: start at ask, move toward bid based on signal strength
        if (signal_strength > 0.5) {
            // Aggressive - hit the bid
            return bid_ticks;
        } else if (signal_strength > 0.3) {
            // Semi-aggressive - between bid and ask
            return ask_ticks - static_cast<uint64_t>((ask_ticks - bid_ticks) * signal_strength);
        } else {
            // Passive - at or slightly below ask
            return ask_ticks - 1; // One tick below ask
        }
    }
}

uint64_t OrderImbalanceStrategy::CalculateOrderQuantity(double signal_strength) const {
    // Base quantity scaled by signal strength
    uint64_t base_qty = GetBaseQuantity();
    
    // Scale by signal strength (minimum 1, maximum 3x base)
    double scale_factor = 1.0 + (signal_strength * 2.0); // 1.0 to 3.0
    uint64_t quantity = static_cast<uint64_t>(base_qty * scale_factor);
    
    // Ensure minimum quantity of 1
    return std::max(1UL, quantity);
}

// L3 Order Book Analysis Methods

double OrderImbalanceStrategy::CalculateL3Imbalance(const OrderBookSnapshot& orderbook_snapshot) const {
    uint64_t total_bid_volume = orderbook_snapshot.GetTotalBidVolume();
    uint64_t total_ask_volume = orderbook_snapshot.GetTotalAskVolume();
    
    if (total_bid_volume + total_ask_volume == 0) {
        return 0.0;
    }
    
    double imbalance = static_cast<double>(total_bid_volume) - static_cast<double>(total_ask_volume);
    double total_volume = static_cast<double>(total_bid_volume + total_ask_volume);
    
    return imbalance / total_volume;
}

double OrderImbalanceStrategy::CalculateWeightedImbalance(const OrderBookSnapshot& orderbook_snapshot, size_t depth) const {
    if (orderbook_snapshot.bid_levels.empty() || orderbook_snapshot.ask_levels.empty()) {
        return 0.0;
    }
    
    double weighted_bid_volume = 0.0;
    double weighted_ask_volume = 0.0;
    
    // Calculate weighted bid volume (higher prices get more weight)
    for (size_t i = 0; i < std::min(depth, orderbook_snapshot.bid_levels.size()); ++i) {
        const auto& level = orderbook_snapshot.bid_levels[i];
        double weight = 1.0 / (1.0 + i);  // Decreasing weight with distance from BBO
        weighted_bid_volume += level.total_volume * weight;
    }
    
    // Calculate weighted ask volume (lower prices get more weight)
    for (size_t i = 0; i < std::min(depth, orderbook_snapshot.ask_levels.size()); ++i) {
        const auto& level = orderbook_snapshot.ask_levels[i];
        double weight = 1.0 / (1.0 + i);  // Decreasing weight with distance from BBO
        weighted_ask_volume += level.total_volume * weight;
    }
    
    if (weighted_bid_volume + weighted_ask_volume == 0.0) {
        return 0.0;
    }
    
    double imbalance = weighted_bid_volume - weighted_ask_volume;
    double total_weighted_volume = weighted_bid_volume + weighted_ask_volume;
    
    return imbalance / total_weighted_volume;
}

double OrderImbalanceStrategy::CalculateOrderCountImbalance(const OrderBookSnapshot& orderbook_snapshot) const {
    uint64_t total_bid_orders = 0;
    uint64_t total_ask_orders = 0;
    
    // Count orders on bid side
    for (const auto& level : orderbook_snapshot.bid_levels) {
        total_bid_orders += level.order_count;
    }
    
    // Count orders on ask side
    for (const auto& level : orderbook_snapshot.ask_levels) {
        total_ask_orders += level.order_count;
    }
    
    if (total_bid_orders + total_ask_orders == 0) {
        return 0.0;
    }
    
    double imbalance = static_cast<double>(total_bid_orders) - static_cast<double>(total_ask_orders);
    double total_orders = static_cast<double>(total_bid_orders + total_ask_orders);
    
    return imbalance / total_orders;
}

double OrderImbalanceStrategy::CalculatePriceLevelDensity(const OrderBookSnapshot& orderbook_snapshot) const {
    if (orderbook_snapshot.bid_levels.empty() || orderbook_snapshot.ask_levels.empty()) {
        return 0.0;
    }
    
    // Calculate density as the ratio of price levels to price range
    double best_bid = orderbook_snapshot.GetBestBid();
    double best_ask = orderbook_snapshot.GetBestAsk();
    
    if (best_bid <= 0 || best_ask <= 0) {
        return 0.0;
    }
    
    // Calculate price range for analysis (use first 10 levels or all available)
    size_t max_levels = 10;
    double bid_range = 0.0;
    double ask_range = 0.0;
    
    if (orderbook_snapshot.bid_levels.size() > 1) {
        size_t levels_to_check = std::min(max_levels, orderbook_snapshot.bid_levels.size());
        double lowest_bid = orderbook_snapshot.bid_levels[levels_to_check - 1].price / 100.0;
        bid_range = best_bid - lowest_bid;
    }
    
    if (orderbook_snapshot.ask_levels.size() > 1) {
        size_t levels_to_check = std::min(max_levels, orderbook_snapshot.ask_levels.size());
        double highest_ask = orderbook_snapshot.ask_levels[levels_to_check - 1].price / 100.0;
        ask_range = highest_ask - best_ask;
    }
    
    // Calculate density metric
    double bid_density = (bid_range > 0) ? orderbook_snapshot.bid_levels.size() / bid_range : 0.0;
    double ask_density = (ask_range > 0) ? orderbook_snapshot.ask_levels.size() / ask_range : 0.0;
    
    // Return density imbalance (higher density on one side indicates more liquidity)
    if (bid_density + ask_density == 0.0) {
        return 0.0;
    }
    
    return (bid_density - ask_density) / (bid_density + ask_density);
}

void OrderImbalanceStrategy::UpdateHistoryFromOrderBook(const OrderBookSnapshot& orderbook_snapshot) {
    // Calculate L3 imbalance for history tracking
    double l3_imbalance = CalculateL3Imbalance(orderbook_snapshot);
    
    // Update imbalance history
    imbalance_history_.push_back(l3_imbalance);
    if (imbalance_history_.size() > lookback_window_) {
        imbalance_history_.pop_front();
    }
    
    // Update price history
    double mid_price = orderbook_snapshot.GetMidPrice();
    if (mid_price > 0.0) {
        price_history_.push_back(mid_price);
        if (price_history_.size() > lookback_window_) {
            price_history_.pop_front();
        }
    }
    
    // Update volume history with total L3 volume
    uint64_t total_volume = orderbook_snapshot.GetTotalBidVolume() + orderbook_snapshot.GetTotalAskVolume();
    volume_history_.push_back(total_volume);
    if (volume_history_.size() > lookback_window_) {
        volume_history_.pop_front();
    }
}