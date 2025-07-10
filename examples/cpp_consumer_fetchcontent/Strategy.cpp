#include "Strategy.h"
#include "PortfolioManager.h"
#include "TopOfBookTracker.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>

// Strategy base class implementation

Strategy::Strategy(const std::string &name, uint64_t user_id,
                   MathFunction math_func,
                   std::shared_ptr<PortfolioManager> portfolio_mgr)
    : name_(name), user_id_(user_id), enabled_(true), signal_threshold_(0.1),
      base_quantity_(1), portfolio_manager_(portfolio_mgr),
      math_function_(math_func) {

  // Set default parameters
  parameters_["max_position"] = 100.0;
  parameters_["risk_multiplier"] = 1.0;
}

StrategyAction Strategy::ProcessMarketData(const MarketSnapshot &snapshot) {
  if (!enabled_ || !math_function_) {
    return StrategyAction(StrategySignal::NONE, 0, 0.0);
  }

  // Calculate signal using mathematical function
  double signal_value = math_function_(snapshot);

  // Convert signal to action
  return SignalToAction(signal_value);
}

StrategyAction Strategy::OnTopOfBookUpdate(const TOBSnapshot &tob_snapshot) {
  // Convert TOBSnapshot to MarketSnapshot
  MarketSnapshot market_snapshot(
      tob_snapshot.timestamp, tob_snapshot.symbol, tob_snapshot.best_bid,
      tob_snapshot.best_ask, tob_snapshot.bid_volume, tob_snapshot.ask_volume);

  return ProcessMarketData(market_snapshot);
}

double Strategy::GetParameter(const std::string &key,
                              double default_value) const {
  auto it = parameters_.find(key);
  return (it != parameters_.end()) ? it->second : default_value;
}

StrategyAction Strategy::SignalToAction(double signal_value) {
  // Clamp signal value to [-1.0, 1.0]
  signal_value = std::max(-1.0, std::min(1.0, signal_value));

  double abs_signal = std::abs(signal_value);

  // Check if signal is strong enough
  if (abs_signal < signal_threshold_) {
    return StrategyAction(StrategySignal::NONE, 0, abs_signal);
  }

  // Check position limits if portfolio manager is available
  uint64_t quantity = base_quantity_;
  if (portfolio_manager_) {
    int64_t current_position = portfolio_manager_->GetRunningPosition();
    double max_position = GetParameter("max_position", 100.0);

    // Adjust quantity based on current position and limits
    if (signal_value > 0 &&
        current_position >= static_cast<int64_t>(max_position)) {
      return StrategyAction(StrategySignal::HOLD, 0, abs_signal);
    }
    if (signal_value < 0 &&
        current_position <= -static_cast<int64_t>(max_position)) {
      return StrategyAction(StrategySignal::HOLD, 0, abs_signal);
    }

    // Scale quantity by risk multiplier and signal strength
    double risk_multiplier = GetParameter("risk_multiplier", 1.0);
    quantity = static_cast<uint64_t>(quantity * risk_multiplier * abs_signal);
    quantity = std::max(1UL, quantity); // Minimum quantity of 1
  }

  // Determine signal direction
  StrategySignal signal =
      (signal_value > 0) ? StrategySignal::BUY : StrategySignal::SELL;

  return StrategyAction(signal, quantity, abs_signal);
}

// OrderImbalanceStrategy implementation

OrderImbalanceStrategy::OrderImbalanceStrategy(
    uint64_t user_id, std::shared_ptr<PortfolioManager> portfolio_mgr,
    double lookback_period)
    : Strategy("OrderImbalance", user_id, CreateOrderImbalanceFunction(0.1),
               portfolio_mgr),
      lookback_period_(lookback_period) {

  // Set strategy-specific parameters
  SetParameter("imbalance_threshold", 0.1);
  SetParameter("momentum_factor", 1.5);
  SetParameter("decay_factor", 0.95);
}

MathFunction
OrderImbalanceStrategy::CreateOrderImbalanceFunction(double threshold) {
  return [threshold](const MarketSnapshot &snapshot) -> double {
    // Basic order imbalance calculation
    double imbalance = snapshot.order_imbalance;

    // Apply threshold and scaling
    if (std::abs(imbalance) < threshold) {
      return 0.0; // Not enough imbalance to act
    }

    // Scale imbalance to signal strength
    // Strong imbalance (>0.5) gets full signal, moderate imbalance gets partial
    double signal_strength = std::min(1.0, std::abs(imbalance) / 0.5);

    // Return signal in direction of imbalance
    return (imbalance > 0) ? signal_strength : -signal_strength;
  };
}

// MeanReversionStrategy implementation

MeanReversionStrategy::MeanReversionStrategy(
    uint64_t user_id, std::shared_ptr<PortfolioManager> portfolio_mgr,
    double lookback_period)
    : Strategy("MeanReversion", user_id, CreateMeanReversionFunction(2.0),
               portfolio_mgr),
      lookback_period_(lookback_period) {

  // Set strategy-specific parameters
  SetParameter("std_dev_threshold", 2.0);
  SetParameter("mean_revert_factor", 0.8);
  SetParameter("max_lookback", lookback_period);
}

MathFunction
MeanReversionStrategy::CreateMeanReversionFunction(double std_dev_threshold) {
  return [std_dev_threshold](const MarketSnapshot &snapshot) -> double {
    // For this simple implementation, we'll use the spread as a proxy for mean
    // reversion In a real implementation, you'd maintain price history and
    // calculate moving averages

    double mid_price = snapshot.mid_price;
    double spread = snapshot.spread;

    if (mid_price <= 0 || spread <= 0) {
      return 0.0;
    }

    // Use spread as a proxy for volatility
    // When spread is wide, assume mean reversion opportunity
    double spread_ratio = spread / mid_price;

    // If spread is unusually wide (>0.1% of mid price), signal mean reversion
    if (spread_ratio > 0.001) {
      // Signal to buy at bid (sell signal) if spread is wide
      // This is a simplified mean reversion signal
      return -std::min(1.0, spread_ratio * 1000.0);
    }

    return 0.0;
  };
}

// StrategyManager implementation

void StrategyManager::AddStrategy(uint64_t user_id,
                                  std::shared_ptr<Strategy> strategy) {
  if (!strategy) {
    return;
  }

  // Remove existing strategy for this user if any
  RemoveStrategy(user_id);

  // Add new strategy
  user_strategies_[user_id] = strategy;
  all_strategies_.push_back(strategy);

  std::cout << "Added " << strategy->GetName() << " strategy for user "
            << user_id << std::endl;
}

void StrategyManager::RemoveStrategy(uint64_t user_id) {
  auto it = user_strategies_.find(user_id);
  if (it != user_strategies_.end()) {
    // Remove from all_strategies_ vector
    auto strategy = it->second;
    all_strategies_.erase(
        std::remove(all_strategies_.begin(), all_strategies_.end(), strategy),
        all_strategies_.end());

    // Remove from user map
    user_strategies_.erase(it);

    std::cout << "Removed strategy for user " << user_id << std::endl;
  }
}

std::shared_ptr<Strategy> StrategyManager::GetStrategy(uint64_t user_id) const {
  auto it = user_strategies_.find(user_id);
  return (it != user_strategies_.end()) ? it->second : nullptr;
}

std::vector<std::pair<uint64_t, StrategyAction>>
StrategyManager::ProcessMarketData(const MarketSnapshot &snapshot) {
  std::vector<std::pair<uint64_t, StrategyAction>> results;

  for (const auto &[user_id, strategy] : user_strategies_) {
    if (strategy && strategy->IsEnabled()) {
      StrategyAction action = strategy->ProcessMarketData(snapshot);
      if (action.signal != StrategySignal::NONE) {
        results.emplace_back(user_id, action);
      }
    }
  }

  return results;
}

void StrategyManager::Clear() {
  user_strategies_.clear();
  all_strategies_.clear();
  std::cout << "Cleared all strategies" << std::endl;
}