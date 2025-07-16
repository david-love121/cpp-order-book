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
    , max_signal_strength_(1.0)
    , trade_volume_imbalance_(0.0)
    , order_client_(nullptr)
    , auto_trading_enabled_(false)
    , last_order_time_(0)   // 15% minimum signal to trade
{  
    // Set reasonable defaults for high-frequency trading
    SetSignalThreshold(0.05);  // Lower threshold for HFT
    SetBaseQuantity(1);       // Moderate base quantity
    SetMaxPosition(10);
    
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
    
    if (!IsEnabled() || (portfolio_manager_ && portfolio_manager_->IsStrategyDisabled())) {
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
    

    
    // Calculate price level density
    //double density_signal = CalculatePriceLevelDensity(orderbook_snapshot);
    
    // Combine multiple L3 signals with different weights
    double combined_signal = 0.7 * l3_imbalance + 0.2 * weighted_imbalance + 0.1 * trade_volume_imbalance_;
                           
    
    // Update historical data using order book snapshot
    UpdateHistoryFromOrderBook(orderbook_snapshot);
    
    // Calculate momentum-adjusted signal
    double momentum_signal = CalculateMomentumSignal(combined_signal);
    
    // Calculate signal strength
    double signal_strength = CalculateSignalStrength(combined_signal);
    double final_signal = momentum_signal;
    StrategyAction action = SignalToAction(final_signal);
    action.confidence = signal_strength;
    // Execute auto-trading if enabled and signal is strong enough
    bool order_placed = false;
    if (auto_trading_enabled_ && action.signal != StrategySignal::NONE && action.signal != StrategySignal::HOLD) {
        order_placed = ExecuteAutoTrade(final_signal, orderbook_snapshot);
    }
    

    
    
    // Log significant L3 signals
    if (std::abs(final_signal) > GetSignalThreshold()) {
        std::cout << "[STRATEGY-L3] L3 signal=" << final_signal 
                  << ", l3_imbalance=" << l3_imbalance
                  << ", weighted=" << weighted_imbalance
                  << ", levels=" << orderbook_snapshot.bid_levels.size() + orderbook_snapshot.ask_levels.size()
                  << ", action=" << (action.signal == StrategySignal::BUY ? "BUY" : 
                                   action.signal == StrategySignal::SELL ? "SELL" : "HOLD")
                  << ", qty=" << action.quantity
                  << ", confidence=" << action.confidence
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
    trade_volume_imbalance_ = 0.0;

    
    // Reset auto-trading state
    last_order_time_ = 0;
    recent_order_times_.clear();
    
    std::cout << "[STRATEGY] OrderImbalanceStrategy reset" << std::endl;
}


void OrderImbalanceStrategy::SetLookbackWindow(size_t window) {
    lookback_window_ = window;

    
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


bool OrderImbalanceStrategy::ExecuteAutoTrade(double signal, const OrderBookSnapshot& orderbook_snapshot) {
    if (!order_client_ || !auto_trading_enabled_) {
        return false;
    }
    // Determine order direction
    bool is_buy = signal > 0.0;
    // Calculate order parameters
    uint64_t quantity_unsigned = CalculateOrderQuantity(std::abs(signal));
    uint64_t price = CalculateOrderPrice(signal, orderbook_snapshot, is_buy);
    int64_t quantity = static_cast<int64_t>(is_buy ? quantity_unsigned : -quantity_unsigned);
    int64_t current_positions = portfolio_manager_->GetRunningPosition();
    //Determine if we can trade without reaching max
    int64_t new_quantity = current_positions + quantity;
    if (signal > 0.0 && new_quantity > static_cast<int64_t>(GetMaxPosition())) {
        std::cout << "[AUTO-TRADE] Cannot place BUY order, would exceed max position limit." << std::endl;
        return false;
    }
    if (signal < 0.0 && new_quantity < -static_cast<int64_t>(GetMaxPosition())) {
        std::cout << "[AUTO-TRADE] Cannot place SELL order, would exceed max position limit." << std::endl;
        return false;
    }

    if (quantity == 0 || price == 0) {
        return false;
    }
    
    // Place the order
    uint64_t synthetic_databento_id = 0;  // Use 0 for strategy-generated orders

    uint64_t order_id = order_client_->SubmitOrder(synthetic_databento_id, GetUserId(), is_buy, quantity_unsigned, price, orderbook_snapshot.timestamp, orderbook_snapshot.timestamp);

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


uint64_t OrderImbalanceStrategy::CalculateOrderPrice(double signal, const OrderBookSnapshot& orderbook_snapshot, bool is_buy) const {
    if (orderbook_snapshot.GetBestBid() <= 0.0 || orderbook_snapshot.GetBestAsk() <= 0.0) {
        return 0;
    }

    uint64_t bid_ticks = static_cast<uint64_t>(orderbook_snapshot.GetBestBid() * 100.0);
    uint64_t ask_ticks = static_cast<uint64_t>(orderbook_snapshot.GetBestAsk() * 100.0);
    uint64_t spread_ticks = ask_ticks - bid_ticks;

    double signal_strength = std::abs(signal);

    // Wide spread: be more passive
    if (spread_ticks > 2) {
        if (is_buy) {
            return bid_ticks + 1;
        } else {
            return ask_ticks - 1;
        }
    }

    // Narrow spread: be more aggressive
    if (is_buy) {
        if (signal_strength > 0.5) {
            return ask_ticks;
        } else {
            return bid_ticks + static_cast<uint64_t>((ask_ticks - bid_ticks) * signal_strength);
        }
    } else {
        if (signal_strength > 0.5) {
            return bid_ticks;
        } else {
            return ask_ticks - static_cast<uint64_t>((ask_ticks - bid_ticks) * signal_strength);
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

void OrderImbalanceStrategy::ProcessTradeData(const Trade& trade) {
    if (trade.aggressor_is_buy) {
        trade_volume_imbalance_ += static_cast<double>(trade.quantity);
    } else {
        trade_volume_imbalance_ -= static_cast<double>(trade.quantity);
    }

    // Decay the trade volume imbalance over time
    trade_volume_imbalance_ *= decay_factor_;
}