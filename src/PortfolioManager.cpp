#include "PortfolioManager.h"
#include "IStrategy.h"
#include "Logger.h"
#include "Trade.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

PortfolioManager::PortfolioManager(uint64_t tracked_user_id,
                                   std::shared_ptr<IDataSink> data_sink)
    : tracked_user_id_(tracked_user_id), data_sink_(std::move(data_sink)) {

  std::cout << "[PORTFOLIO] Initialized for user " << tracked_user_id_
            << " (order ID tracking mode)" << '\n';
}

PortfolioManager::~PortfolioManager() {}

void PortfolioManager::OnOrderSubmitted(uint64_t order_id, uint64_t user_id,
                                        bool is_buy, uint64_t quantity,
                                        uint64_t price, uint64_t timestamp) {
  if (user_id != tracked_user_id_) {
    return;
  }
  if (timestamp == 0) {
    timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count();
  }
  tracked_order_ids_.insert(order_id);
  tracked_orders_.emplace(
      order_id, TrackedOrder(order_id, is_buy, quantity, price, timestamp));
}

void PortfolioManager::OnTrade(const Trade &trade) {
  *GLogger << "[PortfolioManager] OnTrade received trade: " << trade.ToString() << '\n';
  if (trade.aggressor_user_id != tracked_user_id_ && trade.resting_user_id != tracked_user_id_) {
    *GLogger << "[PortfolioManager] Trade does not involve tracked user " << tracked_user_id_ << ". Ignoring." << '\n';
    return; // Not our trade
  }
  *GLogger << "[PortfolioManager] Trade involves tracked user " << tracked_user_id_ << ". Processing." << '\n';
  trades_.push_back(trade);
  bool is_buy = trade.aggressor_user_id == tracked_user_id_ ? trade.aggressor_is_buy : !trade.aggressor_is_buy;
  double trade_price = static_cast<double>(trade.price);
  int64_t trade_quantity = static_cast<int64_t>(trade.quantity);

  current_market_price_ = trade_price;
  total_trades_++;

  if (running_position_ > 0) {
    HandleLongPositionTrade(is_buy, trade_price, trade_quantity);
  } else if (running_position_ < 0) {
    HandleShortPositionTrade(is_buy, trade_price, trade_quantity);
  } else {
    // No existing position, so this trade opens one
    if (is_buy) {
      HandleLongPositionTrade(is_buy, trade_price, trade_quantity);
    } else {
      HandleShortPositionTrade(is_buy, trade_price, trade_quantity);
    }
  }
  
  CheckStopLoss();
  CheckDailyLossLimit();
  
  TakeSnapshot(trade.ts_executed);

  if (data_sink_) {
    data_sink_->OnData(trade);
  }
}

void PortfolioManager::UpdateMarketPrice(double price, uint64_t timestamp) {
  current_market_price_ = price;

  if (timestamp == 0) {
    timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count();
  }

  // Check for periodic snapshots
  if (periodic_snapshots_enabled_ &&
      (timestamp - last_snapshot_timestamp_) >= snapshot_interval_ns_) {
    TakeSnapshot(timestamp);
    last_snapshot_timestamp_ = timestamp;
  }

  // Take snapshot on price updates if we have a position
  if (running_position_ != 0) {
    CheckStopLoss();
    TakeSnapshot(timestamp);
  }
}

void PortfolioManager::ForceSnapshot(uint64_t timestamp) {
  if (timestamp == 0) {
    timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count();
  }
  TakeSnapshot(timestamp);
}

void PortfolioManager::PrintPortfolioSummary() const {
  std::cout << "\n=== Portfolio Summary (User " << tracked_user_id_
            << ") ===" << '\n';
  std::cout << "Tracked Orders: " << tracked_order_ids_.size() << '\n';
  std::cout << "Running Position: " << running_position_ << " contracts"
            << '\n';
  std::cout << "Current Market Price: $" << std::fixed << std::setprecision(2)
            << (current_market_price_ / 100.0) << '\n';
  std::cout << "Average Cost: $" << std::fixed << std::setprecision(2)
            << (average_cost_ / 100.0) << '\n';
  std::cout << "Position Value: $" << std::fixed << std::setprecision(2)
            << ((current_market_price_ * std::abs(running_position_)) / 100.0)
            << '\n';
  std::cout << "Realized P&L: $" << std::fixed << std::setprecision(2)
            << (realized_pnl_ / 100.0) << '\n';
  std::cout << "Unrealized P&L: $" << std::fixed << std::setprecision(2)
            << (CalculateUnrealizedPnL() / 100.0) << '\n';
  std::cout << "Total P&L: $" << std::fixed << std::setprecision(2)
            << (GetTotalPnL() / 100.0) << '\n';

  if (GetTotalCostBasis() != 0.0) {
    double return_pct = (GetTotalPnL() / GetTotalCostBasis()) * 100.0;
    std::cout << "Return on Equity: " << std::fixed << std::setprecision(2)
              << return_pct << "%" << '\n';
  }

  std::cout << "Total Trades: " << total_trades_ << '\n';

  // Add risk metrics
  auto risk_metrics = CalculateRiskMetrics();
  std::cout << "\n--- Risk Metrics ---" << '\n';
  std::cout << "Max Position Value: $" << std::fixed << std::setprecision(2)
            << (risk_metrics.max_position_value / 100.0) << '\n';
  std::cout << "Volatility: " << std::fixed << std::setprecision(4)
            << risk_metrics.volatility << '\n';
  std::cout << "Sharpe Ratio: " << std::fixed << std::setprecision(4)
            << risk_metrics.sharpe_ratio << '\n';
  std::cout << "Max Drawdown: $" << std::fixed << std::setprecision(2)
            << (risk_metrics.max_drawdown / 100.0) << '\n';
  std::cout << "VaR 95%: " << std::fixed << std::setprecision(4)
            << risk_metrics.var_95 << '\n';

  // Add performance statistics
  auto perf_stats = GetPerformanceStats();
  std::cout << "\n--- Performance Statistics ---" << '\n';
  std::cout << "Win Rate: " << std::fixed << std::setprecision(2)
            << (perf_stats.win_rate * 100.0) << "%" << '\n';
  std::cout << "Winning Trades: " << perf_stats.winning_trades << '\n';
  std::cout << "Losing Trades: " << perf_stats.losing_trades << '\n';
  std::cout << "Average Win: $" << std::fixed << std::setprecision(2)
            << (perf_stats.avg_win / 100.0) << '\n';
  std::cout << "Average Loss: $" << std::fixed << std::setprecision(2)
            << (perf_stats.avg_loss / 100.0) << '\n';
  std::cout << "Profit Factor: " << std::fixed << std::setprecision(2)
            << perf_stats.profit_factor << '\n';
  std::cout << "Largest Win: $" << std::fixed << std::setprecision(2)
            << (perf_stats.largest_win / 100.0) << '\n';
  std::cout << "Largest Loss: $" << std::fixed << std::setprecision(2)
            << (perf_stats.largest_loss / 100.0) << '\n';

  if (!tracked_order_ids_.empty()) {
    std::cout << "\n--- Tracked Order IDs ---" << '\n';
    for (uint64_t order_id : tracked_order_ids_) {
      auto it = tracked_orders_.find(order_id);
      if (it != tracked_orders_.end()) {
        const auto &order = it->second;
        std::cout << "  Order " << order_id << ": "
                  << (order.is_buy ? "BUY" : "SELL") << " " << order.quantity
                  << " @ " << order.price
                  << " (remaining: " << order.remaining_quantity << ")"
                  << '\n';
      }
    }
  }

  std::cout << "=============================================" << '\n';
}



std::string PortfolioManager::TimestampToString(uint64_t timestamp_ns) const {
  // Convert nanoseconds to seconds for standard time conversion
  auto timestamp_seconds = std::chrono::seconds(timestamp_ns / 1000000000);
  auto timestamp_nanos_remainder = timestamp_ns % 1000000000;

  // Convert to time_point
  auto time_point = std::chrono::system_clock::time_point(timestamp_seconds);

  // Convert to time_t for formatting
  std::time_t time_t_val = std::chrono::system_clock::to_time_t(time_point);

  // Format as ISO 8601 string with nanosecond precision
  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t_val), "%Y-%m-%dT%H:%M:%S");
  oss << "." << std::setfill('0') << std::setw(9) << timestamp_nanos_remainder
      << "Z";

  return oss.str();
}


double PortfolioManager::GetTotalCostBasis() const {
  if (running_position_ == 0) {
    return 0.0;
  }

  double total_cost_basis = 0.0;

  // Sum up the net cost basis from all executed orders
  // For buys: add cost (cash outflow)
  // For sells: subtract proceeds (cash inflow)
  for (const auto& [order_id, order] : tracked_orders_) {
    uint64_t executed_quantity = order.quantity - order.remaining_quantity;
    if (executed_quantity > 0) {
      double order_value = static_cast<double>(executed_quantity * order.price);
      if (order.is_buy) {
        total_cost_basis += order_value;  // Cash spent on purchases
      } else {
        total_cost_basis -= order_value;  // Cash received from sales
      }
    }
  }

  // Return absolute value since cost basis represents total investment magnitude
  return std::abs(total_cost_basis);
}

double PortfolioManager::GetReturnOnEquity() const {
  double cost_basis = GetTotalCostBasis();
  return cost_basis != 0.0 ? GetTotalPnL() / cost_basis : 0.0;
}

double PortfolioManager::CalculateUnrealizedPnL() const {
  if (running_position_ == 0) {
    return 0.0;
  }
  // For short positions, PnL is (entry_price - current_price)
  // which is the negative of the long position calculation.
  return (current_market_price_ - average_cost_) * running_position_;
}

void PortfolioManager::HandleLongPositionTrade(bool is_buy, double trade_price,
                                             int64_t trade_quantity) {
  if (is_buy) {
    // Increasing a long position
    double current_value = average_cost_ * running_position_;
    double trade_value = trade_price * trade_quantity;
    running_position_ += trade_quantity;
    average_cost_ = (current_value + trade_value) / running_position_;
  } else {
    // Reducing or closing a long position
    double pnl = (trade_price - average_cost_) * trade_quantity;
    realized_pnl_ += pnl;
    running_position_ -= trade_quantity;
    if (running_position_ == 0) {
      average_cost_ = 0.0;
    }
  }
}

void PortfolioManager::HandleShortPositionTrade(bool is_buy, double trade_price,
                                              int64_t trade_quantity) {
  if (!is_buy) {
    // Opening or increasing a short position
    double current_value = average_cost_ * std::abs(running_position_);
    double trade_value = trade_price * trade_quantity;
    running_position_ -= trade_quantity;
    average_cost_ = (current_value + trade_value) / std::abs(running_position_);
  } else {
    // Closing or reducing a short position
    double pnl = (average_cost_ - trade_price) * trade_quantity;
    realized_pnl_ += pnl;
    running_position_ += trade_quantity;
    if (running_position_ == 0) {
      average_cost_ = 0.0;
    }
  }
}

void PortfolioManager::TakeSnapshot(uint64_t timestamp) {
  if (timestamp == 0) {
    timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count();
  }

  PortfolioSnapshot snapshot(timestamp, running_position_,
                             current_market_price_, average_cost_,
                             CalculateUnrealizedPnL(), realized_pnl_,
                             total_trades_);

  snapshots_.push_back(snapshot);

  if (data_sink_) {
    data_sink_->OnData(snapshot);
  }
}

void PortfolioManager::EnablePeriodicSnapshots(uint64_t interval_ns) {
  snapshot_interval_ns_ = interval_ns;
  periodic_snapshots_enabled_ = true;
  last_snapshot_timestamp_ =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count();

  std::cout << "[PORTFOLIO] Periodic snapshots enabled (interval: "
            << (interval_ns / 1000000) << "ms)" << '\n';
}

void PortfolioManager::DisablePeriodicSnapshots() {
  periodic_snapshots_enabled_ = false;
  snapshot_interval_ns_ = 0;
  std::cout << "[PORTFOLIO] Periodic snapshots disabled" << '\n';
}



void PortfolioManager::Reset() {
  tracked_order_ids_.clear();
  tracked_orders_.clear();
  running_position_ = 0;
  realized_pnl_ = 0.0;
  average_cost_ = 0.0;
  current_market_price_ = 0.0;
  total_trades_ = 0;
  snapshots_.clear();
  last_snapshot_timestamp_ = 0;
  strategy_disabled_ = false;

  std::cout << "[PORTFOLIO] Portfolio state reset for user " << tracked_user_id_
            << '\n';

}

void PortfolioManager::OnOrderCancelled(uint64_t order_id) {
  auto it = tracked_order_ids_.find(order_id);
  if (it != tracked_order_ids_.end()) {
    tracked_order_ids_.erase(it);
    tracked_orders_.erase(order_id);

    std::cout << "[PORTFOLIO] Order " << order_id
              << " cancelled and removed from tracking" << '\n';
  }
}

void PortfolioManager::OnOrderModified(uint64_t order_id, uint64_t new_quantity,
                                       uint64_t new_price) {
  auto it = tracked_orders_.find(order_id);
  if (it != tracked_orders_.end()) {
    uint64_t old_quantity = it->second.quantity;
    uint64_t old_price = it->second.price;

    it->second.quantity = new_quantity;
    it->second.remaining_quantity = new_quantity;
    it->second.price = new_price;

    std::cout << "[PORTFOLIO] Order " << order_id
              << " modified: " << old_quantity << "@" << old_price << " -> "
              << new_quantity << "@" << new_price << '\n';
  }
}
const TrackedOrder *PortfolioManager::GetOrderDetails(uint64_t order_id) const {
  auto it = tracked_orders_.find(order_id);
  return (it != tracked_orders_.end()) ? &it->second : nullptr;
}

const std::vector<Trade>& PortfolioManager::GetTrades() const {
    return trades_;
}

bool PortfolioManager::IsStrategyDisabled() const {
  return strategy_disabled_;
}

void PortfolioManager::CheckStopLoss() {
    if (stop_loss_percentage_ == 0.0 || running_position_ == 0) {
        return;
    }

    double unrealized_pnl = CalculateUnrealizedPnL();
    double position_value = GetPositionValue();

    if (position_value > 0 && (unrealized_pnl / position_value) < -stop_loss_percentage_) {
        std::cout << "[PORTFOLIO] Stop-loss triggered! Unrealized PnL: " << unrealized_pnl
                  << ", Position Value: " << position_value << '\n';
        // This is a simplified implementation. A real implementation would
        // need to create and submit an order to close the position.
        // For now, we'll just disable the strategy.
        strategy_disabled_ = true;
    }
}

void PortfolioManager::CheckDailyLossLimit() {
    if (max_daily_loss_ == 0.0) {
        return;
    }

    if (realized_pnl_ < -max_daily_loss_) {
        std::cout << "[PORTFOLIO] Maximum daily loss limit reached! Realized PnL: "
                  << realized_pnl_ << '\n';
        strategy_disabled_ = true;
    }
}

RiskMetrics PortfolioManager::CalculateRiskMetrics() const {
  RiskMetrics metrics{};

  if (snapshots_.size() < 2) {
    return metrics;
  }

  for (const auto &snapshot : snapshots_) {
    double pos_value = std::abs(snapshot.position_value);
    metrics.max_position_value =
        std::max(metrics.max_position_value, pos_value);
  }

  if (metrics.max_position_value < 1e-6) {
    return metrics; // Avoid division by zero if no position was ever taken
  }

  std::vector<double> returns;
  for (size_t i = 1; i < snapshots_.size(); ++i) {
    double pnl_change = snapshots_[i].total_pnl - snapshots_[i - 1].total_pnl;
    double ret = pnl_change / metrics.max_position_value;
    returns.push_back(ret);
  }

  if (!returns.empty()) {
    double mean_return = 0.0;
    for (double ret : returns) {
      mean_return += ret;
    }
    mean_return /= returns.size();

    double variance = 0.0;
    for (double ret : returns) {
      variance += (ret - mean_return) * (ret - mean_return);
    }
    variance /= (returns.size() > 1 ? returns.size() - 1 : 1); // Use sample stdev
    metrics.volatility = std::sqrt(variance);

    if (metrics.volatility > 1e-9) {
      metrics.sharpe_ratio = mean_return / metrics.volatility;
    }
  }

  std::vector<double> pnl_series;
  pnl_series.reserve(snapshots_.size());
  for (const auto &snapshot : snapshots_) {
    pnl_series.push_back(snapshot.total_pnl);
  }

  double peak_pnl = pnl_series[0];
  double max_dd = 0.0;

  for (double pnl : pnl_series) {
    if (pnl > peak_pnl) {
      peak_pnl = pnl;
    }
    double drawdown = peak_pnl - pnl;
    max_dd = std::max(max_dd, drawdown);
  }
  metrics.max_drawdown = max_dd;

  if (returns.size() > 20) {
    std::vector<double> sorted_returns = returns;
    std::sort(sorted_returns.begin(), sorted_returns.end());
    size_t var_index = static_cast<size_t>(0.05 * sorted_returns.size());
    metrics.var_95 = sorted_returns[var_index];
  }

  return metrics;
}

bool PortfolioManager::ExportData(const std::string &format,
                                  const std::string &filename) const {
  if (format == "csv") {
    if (snapshots_.empty()) {
      std::cout << "[PORTFOLIO] No data to export" << '\n';
      return false;
    }

    std::ofstream export_file(filename);
    if (!export_file.is_open()) {
      std::cout << "[PORTFOLIO] Failed to open export file: " << filename
                << '\n';
      return false;
    }

    export_file
        << "timestamp,position,current_price,average_cost,unrealized_pnl,"
           "realized_pnl,total_pnl,total_trades,position_"
           "value\n";

    for (const auto &snapshot : snapshots_) {
      std::string timestamp_str = TimestampToString(snapshot.timestamp);
      export_file << timestamp_str << "," << snapshot.position << ","
                  << std::fixed << std::setprecision(2)
                  << (snapshot.current_price / 100.0) << "," << (snapshot.average_cost / 100.0)
                  << "," << (snapshot.unrealized_pnl / 100.0) << ","
                  << (snapshot.realized_pnl / 100.0) << "," << (snapshot.total_pnl / 100.0) << ","
                  << (snapshot.position_value / 100.0) << "," << "\n";
    }

    export_file.close();
    std::cout << "[PORTFOLIO] Data exported to: " << filename << '\n';
    return true;

  } 
  
  

  std::cout << "[PORTFOLIO] Unsupported export format: " << format << '\n';
  return false;
}

PerformanceStats PortfolioManager::GetPerformanceStats() const {
  PerformanceStats stats{};

  if (snapshots_.size() < 2) {
    return stats;
  }

  std::vector<double> trade_pnls;

  for (size_t i = 1; i < snapshots_.size(); ++i) {
    double pnl_change = snapshots_[i].total_pnl - snapshots_[i - 1].total_pnl;
    if (std::abs(pnl_change) > 1e-6) {
      trade_pnls.push_back(pnl_change);
    }
  }

  if (trade_pnls.empty()) {
    return stats;
  }

  double total_wins = 0.0;
  double total_losses = 0.0;

  for (double pnl : trade_pnls) {
    if (pnl > 0) {
      stats.winning_trades++;
      total_wins += pnl;
      stats.largest_win = std::max(stats.largest_win, pnl);
    } else if (pnl < 0) {
      stats.losing_trades++;
      total_losses += std::abs(pnl);
      stats.largest_loss = std::min(stats.largest_loss, pnl);
    }
  }

  size_t total_trades = stats.winning_trades + stats.losing_trades;
  if (total_trades > 0) {
    stats.win_rate = static_cast<double>(stats.winning_trades) / total_trades;
  }

  if (stats.winning_trades > 0) {
    stats.avg_win = total_wins / stats.winning_trades;
  }

  if (stats.losing_trades > 0) {
    stats.avg_loss = total_losses / stats.losing_trades;
  }

  if (stats.avg_loss > 1e-6) {
    stats.profit_factor = stats.avg_win / stats.avg_loss;
  }

  return stats;
}