#include "IStrategy.h"
#include "PortfolioManager.h"
#include "TopOfBookTracker.h"
#include <algorithm>
#include <cmath>
#include <iostream>

// Strategy base class implementation

Strategy::Strategy(const std::string &name, uint64_t user_id)
    : name_(name), user_id_(user_id), enabled_(true), signal_threshold_(0.1),
      base_quantity_(1), portfolio_manager_(nullptr), risk_multiplier_(1.0), max_position_(10) {



}

// Default implementation is now removed - derived classes must implement ProcessOrderBookData



void Strategy::Reset() {
  // Reset to default state
  enabled_ = true;
  signal_threshold_ = 0.1;
  base_quantity_ = 1;
  risk_multiplier_ = 1.0;
  max_position_ = 10;
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

void Strategy::SetRiskMultiplier(double multiplier) {
  risk_multiplier_ = multiplier;
}

double Strategy::GetRiskMultiplier() const {
  return risk_multiplier_;
}

void Strategy::SetMaxPosition(uint64_t max_pos) {
  max_position_ = max_pos;
}

uint64_t Strategy::GetMaxPosition() const {
  return max_position_;
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


    // Adjust quantity based on current position and limits
    if (signal_value > 0 &&
        current_position >= static_cast<int64_t>(max_position_)) {
      return StrategyAction(StrategySignal::HOLD, 0, abs_signal);
    }
    if (signal_value < 0 &&
        current_position <= -static_cast<int64_t>(max_position_)) {
      return StrategyAction(StrategySignal::HOLD, 0, abs_signal);
    }

    // Scale quantity by risk multiplier and signal strength
    quantity = static_cast<uint64_t>(quantity * risk_multiplier_ * abs_signal);
    quantity = std::max(1UL, quantity); // Minimum quantity of 1
  }

  // Determine signal direction
  StrategySignal signal =
      (signal_value > 0) ? StrategySignal::BUY : StrategySignal::SELL;

  return StrategyAction(signal, quantity, abs_signal);
}


