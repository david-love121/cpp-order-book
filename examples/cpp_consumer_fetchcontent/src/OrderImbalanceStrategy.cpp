#include "OrderImbalanceStrategy.h"
#include "PortfolioManager.h"
#include "DatabentoMboClient.h"
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

StrategyAction OrderImbalanceStrategy::ProcessMarketData(const MarketSnapshot& snapshot) {
    static uint64_t total_calls = 0;
    total_calls++;
    
    if (total_calls <= 5 || total_calls % 1000 == 0) {
        std::cout << "[STRATEGY-ENTRY] ProcessMarketData called #" << total_calls 
                  << " for symbol=" << snapshot.symbol 
                  << ", enabled=" << IsEnabled() << std::endl;
    }
    
    if (!IsEnabled()) {
        return StrategyAction(StrategySignal::NONE, 0, 0.0);
    }
    
    // Check if market conditions are suitable for trading
    if (!IsMarketConditionsSuitable(snapshot)) {
        return StrategyAction(StrategySignal::NONE, 0, 0.0);
    }
    
    // Update historical data
    UpdateHistory(snapshot);
    
    // Calculate raw imbalance
    double raw_imbalance = CalculateImbalance(snapshot);
    
    // Calculate momentum-adjusted signal
    double momentum_signal = CalculateMomentumSignal(raw_imbalance);
    
    // Calculate signal strength based on imbalance pattern
    double signal_strength = CalculateSignalStrength(raw_imbalance);
    
    // Apply risk management
    double final_signal = ApplyRiskManagement(momentum_signal);
    
    // Update current signal state
    current_signal_ = final_signal;
    last_signal_time_ = snapshot.timestamp;
    
    // Execute auto-trading if enabled and signal is strong enough
    bool order_placed = false;
    if (auto_trading_enabled_ && std::abs(final_signal) >= min_signal_for_trade_) {
        order_placed = ExecuteAutoTrade(final_signal, snapshot);
    }
    
    // Convert signal to action using base class helper
    StrategyAction action = SignalToAction(final_signal);
    action.confidence = signal_strength;
    
    // Periodic debug output to show signal values (every 100 calls)
    static uint64_t call_count = 0;
    call_count++;
    if (call_count % 100 == 0) {
        std::cout << "[STRATEGY-DEBUG] Call #" << call_count 
                  << " signal=" << final_signal 
                  << ", raw_imbalance=" << raw_imbalance
                  << ", threshold=" << GetSignalThreshold()
                  << ", min_for_trade=" << min_signal_for_trade_
                  << ", auto_enabled=" << auto_trading_enabled_ << std::endl;
    }
    
    // Log significant signals for debugging
    if (std::abs(final_signal) > GetSignalThreshold()) {
        std::cout << "[STRATEGY] OrderImbalance signal=" << final_signal 
                  << ", imbalance=" << raw_imbalance
                  << ", momentum=" << signal_momentum_
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

double OrderImbalanceStrategy::CalculateImbalance(const MarketSnapshot& snapshot) const {
    // Standard order imbalance calculation
    if (snapshot.bid_volume + snapshot.ask_volume == 0) {
        return 0.0;
    }
    
    double imbalance = static_cast<double>(snapshot.bid_volume) - static_cast<double>(snapshot.ask_volume);
    double total_volume = static_cast<double>(snapshot.bid_volume + snapshot.ask_volume);
    
    return imbalance / total_volume;  // Normalized to [-1.0, 1.0]
}

double OrderImbalanceStrategy::CalculateMomentumSignal(double raw_imbalance) {
    // Update momentum with exponential decay
    signal_momentum_ = signal_momentum_ * decay_factor_ + raw_imbalance * (1.0 - decay_factor_);
    
    // Calculate momentum-enhanced signal
    double momentum_enhanced = raw_imbalance + (signal_momentum_ * momentum_factor_);
    
    // Clamp to reasonable bounds
    return std::max(-max_signal_strength_, std::min(max_signal_strength_, momentum_enhanced));
}

void OrderImbalanceStrategy::UpdateHistory(const MarketSnapshot& snapshot) {
    // Update imbalance history
    double imbalance = CalculateImbalance(snapshot);
    imbalance_history_.push_back(imbalance);
    if (imbalance_history_.size() > lookback_window_) {
        imbalance_history_.pop_front();
    }
    
    // Update price history
    if (snapshot.mid_price > 0.0) {
        price_history_.push_back(snapshot.mid_price);
        if (price_history_.size() > lookback_window_) {
            price_history_.pop_front();
        }
    }
    
    // Update volume history
    uint64_t total_volume = snapshot.bid_volume + snapshot.ask_volume;
    volume_history_.push_back(total_volume);
    if (volume_history_.size() > lookback_window_) {
        volume_history_.pop_front();
    }
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

bool OrderImbalanceStrategy::IsMarketConditionsSuitable(const MarketSnapshot& snapshot) const {
    // Check for valid prices
    if (snapshot.best_bid <= 0.0 || snapshot.best_ask <= 0.0) {
        return false;
    }
    
    // Check for reasonable spread
    if (snapshot.spread <= 0.0 || snapshot.spread > snapshot.mid_price * 0.01) {  // Max 1% spread
        return false;
    }
    
    // Check for minimum volume
    if (snapshot.bid_volume == 0 && snapshot.ask_volume == 0) {
        return false;
    }
    
    // Check for crossed market
    if (snapshot.best_bid >= snapshot.best_ask) {
        return false;
    }
    
    return true;
}

// Auto-trading implementation

bool OrderImbalanceStrategy::ExecuteAutoTrade(double signal, const MarketSnapshot& snapshot) {
    if (!order_client_ || !auto_trading_enabled_) {
        return false;
    }
    
    // Check rate limiting
    if (!CanPlaceOrder(snapshot.timestamp)) {
        return false;
    }
    
    // Determine order direction
    bool is_buy = signal > 0.0;
    
    // Calculate order parameters
    uint64_t quantity = CalculateOrderQuantity(std::abs(signal));
    uint64_t price = CalculateOrderPrice(signal, snapshot, is_buy);
    
    if (quantity == 0 || price == 0) {
        return false;
    }
    
    // Place the order
    uint64_t order_id = order_client_->SubmitOrder(GetUserId(), is_buy, quantity, price);
    
    if (order_id > 0) {
        // Record successful order placement
        recent_order_times_.push_back(snapshot.timestamp);
        last_order_time_ = snapshot.timestamp;
        
        // Clean up old timestamps (keep only last minute)
        uint64_t one_minute_ago = snapshot.timestamp - 60000000000ULL; // 60 seconds in nanoseconds
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

uint64_t OrderImbalanceStrategy::CalculateOrderPrice(double signal, const MarketSnapshot& snapshot, bool is_buy) const {
    if (snapshot.best_bid <= 0.0 || snapshot.best_ask <= 0.0) {
        return 0;
    }
    
    // Convert prices to ticks (assuming 0.01 tick size, multiply by 100)
    uint64_t bid_ticks = static_cast<uint64_t>(snapshot.best_bid * 100.0);
    uint64_t ask_ticks = static_cast<uint64_t>(snapshot.best_ask * 100.0);
    
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