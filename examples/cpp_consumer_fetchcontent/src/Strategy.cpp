#include "IStrategy.h"
#include "PortfolioManager.h"
#include "TopOfBookTracker.h"
#include <algorithm>
#include <cmath>
#include <iostream>

// Strategy base class implementation

Strategy::Strategy(const std::string &name, uint64_t user_id)
    : name_(name), user_id_(user_id), enabled_(true), signal_threshold_(0.1),
      base_quantity_(1), portfolio_manager_(nullptr) {

  // Set default parameters
  parameters_["max_position"] = 100.0;
  parameters_["risk_multiplier"] = 1.0;
}

StrategyAction Strategy::OnTopOfBookUpdate(const TOBSnapshot &tob_snapshot) {
  // Convert TOBSnapshot to MarketSnapshot
  MarketSnapshot market_snapshot(
      tob_snapshot.timestamp, tob_snapshot.symbol, tob_snapshot.best_bid,
      tob_snapshot.best_ask, tob_snapshot.bid_volume, tob_snapshot.ask_volume);

  return ProcessMarketData(market_snapshot);
}

void Strategy::Initialize(const std::unordered_map<std::string, double> &parameters) {
  parameters_ = parameters;
}

void Strategy::Reset() {
  // Reset to default state
  enabled_ = true;
  signal_threshold_ = 0.1;
  base_quantity_ = 1;
  
  // Reset to default parameters
  parameters_.clear();
  parameters_["max_position"] = 100.0;
  parameters_["risk_multiplier"] = 1.0;
}

void Strategy::SetEnabled(bool enabled) {
  enabled_ = enabled;
}

void Strategy::SetPortfolioManager(std::shared_ptr<PortfolioManager> portfolio_mgr) {
  portfolio_manager_ = portfolio_mgr;
}

const std::string &Strategy::GetName() const {
  return name_;
}

uint64_t Strategy::GetUserId() const {
  return user_id_;
}

bool Strategy::IsEnabled() const {
  return enabled_;
}

std::shared_ptr<PortfolioManager> Strategy::GetPortfolioManager() const {
  return portfolio_manager_;
}

void Strategy::SetParameter(const std::string &key, double value) {
  parameters_[key] = value;
}

double Strategy::GetParameter(const std::string &key, double default_value) const {
  auto it = parameters_.find(key);
  return (it != parameters_.end()) ? it->second : default_value;
}

void Strategy::SetSignalThreshold(double threshold) {
  signal_threshold_ = threshold;
}

double Strategy::GetSignalThreshold() const {
  return signal_threshold_;
}

void Strategy::SetBaseQuantity(uint64_t quantity) {
  base_quantity_ = quantity;
}

uint64_t Strategy::GetBaseQuantity() const {
  return base_quantity_;
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


// StrategyManager implementation

void StrategyManager::AddStrategy(uint64_t user_id,
                                  std::shared_ptr<IStrategy> strategy) {
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

std::shared_ptr<IStrategy> StrategyManager::GetStrategy(uint64_t user_id) const {
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