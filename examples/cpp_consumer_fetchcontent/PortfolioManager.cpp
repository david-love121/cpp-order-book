#include "PortfolioManager.h"
#include "../../include/Trade.h"
#include "Strategy.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

PortfolioManager::PortfolioManager(const std::string &csv_filename)
    : csv_filename_(csv_filename), csv_enabled_(!csv_filename.empty()) {

  std::cout << "[PORTFOLIO] Initialized for user " << TRACKED_USER_ID
            << " (order ID tracking mode)" << std::endl;

  if (csv_enabled_) {
    EnableCSV(csv_filename);
  }
}

PortfolioManager::~PortfolioManager() {
  if (csv_file_.is_open()) {
    csv_file_.flush(); // Ensure all data is written
    csv_file_.close();
  }
}

void PortfolioManager::OnOrderSubmitted(uint64_t order_id, uint64_t user_id,
                                        bool is_buy, uint64_t quantity,
                                        uint64_t price, uint64_t timestamp) {
  if (user_id != TRACKED_USER_ID) {
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

  std::cout << "[PORTFOLIO] Tracking order " << order_id << " for user "
            << user_id << " (" << (is_buy ? "BUY" : "SELL") << " " << quantity
            << " @ " << price << ")" << std::endl;
}

void PortfolioManager::OnTradeExecuted(const Trade &trade) {
  bool aggressor_tracked =
      tracked_order_ids_.count(trade.aggressor_order_id) > 0;
  bool resting_tracked = tracked_order_ids_.count(trade.resting_order_id) > 0;

  if (!aggressor_tracked && !resting_tracked) {
    current_market_price_ = static_cast<double>(trade.price);
    return;
  }

  current_market_price_ = static_cast<double>(trade.price);

  int64_t position_change = 0;
  double trade_cost = 0.0;

  if (aggressor_tracked) {
    auto it = tracked_orders_.find(trade.aggressor_order_id);
    if (it != tracked_orders_.end()) {
      int64_t qty_change = it->second.is_buy
                               ? static_cast<int64_t>(trade.quantity)
                               : -static_cast<int64_t>(trade.quantity);
      position_change += qty_change;

      it->second.remaining_quantity -= trade.quantity;

      if (it->second.is_buy) {
        trade_cost += static_cast<double>(trade.quantity * trade.price);
      } else {
        trade_cost -= static_cast<double>(trade.quantity * trade.price);
      }
    }
  }

  if (resting_tracked) {
    auto it = tracked_orders_.find(trade.resting_order_id);
    if (it != tracked_orders_.end()) {
      int64_t qty_change = it->second.is_buy
                               ? static_cast<int64_t>(trade.quantity)
                               : -static_cast<int64_t>(trade.quantity);
      position_change += qty_change;

      it->second.remaining_quantity -= trade.quantity;

      if (it->second.is_buy) {
        trade_cost += static_cast<double>(trade.quantity * trade.price);
      } else {
        trade_cost -= static_cast<double>(trade.quantity * trade.price);
      }
    }
  }

  if ((running_position_ > 0 && position_change < 0) ||
      (running_position_ < 0 && position_change > 0)) {
    double avg_cost = CalculateAverageCost();
    double realized_qty =
        std::min(static_cast<double>(std::abs(position_change)),
                 static_cast<double>(std::abs(running_position_)));

    if (running_position_ > 0) {
      realized_pnl_ += realized_qty * (current_market_price_ - avg_cost);
    } else {
      realized_pnl_ += realized_qty * (avg_cost - current_market_price_);
    }
  }

  int64_t old_position = running_position_;
  running_position_ += position_change;

  if (position_change > 0) {
    total_cost_basis_ += std::abs(trade_cost);
  } else if (position_change < 0) {
    if (old_position != 0) {
      double reduction_ratio = static_cast<double>(std::abs(position_change)) /
                               static_cast<double>(std::abs(old_position));
      total_cost_basis_ *= (1.0 - reduction_ratio);
    }
  }

  if (running_position_ == 0) {
    total_cost_basis_ = 0.0;
  }

  total_trades_++;

  std::cout << "[PORTFOLIO] Trade executed involving tracked order(s). "
               "Position change: "
            << (position_change >= 0 ? "+" : "") << position_change
            << ", Running position: " << running_position_
            << ", Price: " << current_market_price_ << std::endl;

  TakeSnapshot(trade.ts_executed);
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
  std::cout << "\n=== Portfolio Summary (User " << TRACKED_USER_ID
            << ") ===" << std::endl;
  std::cout << "Tracked Orders: " << tracked_order_ids_.size() << std::endl;
  std::cout << "Running Position: " << running_position_ << " contracts"
            << std::endl;
  std::cout << "Current Market Price: $" << std::fixed << std::setprecision(2)
            << current_market_price_ << std::endl;
  std::cout << "Average Cost: $" << std::fixed << std::setprecision(2)
            << CalculateAverageCost() << std::endl;
  std::cout << "Total Cost Basis: $" << std::fixed << std::setprecision(2)
            << total_cost_basis_ << std::endl;
  std::cout << "Position Value: $" << std::fixed << std::setprecision(2)
            << (current_market_price_ * std::abs(running_position_))
            << std::endl;
  std::cout << "Realized P&L: $" << std::fixed << std::setprecision(2)
            << realized_pnl_ << std::endl;
  std::cout << "Unrealized P&L: $" << std::fixed << std::setprecision(2)
            << CalculateUnrealizedPnL() << std::endl;
  std::cout << "Total P&L: $" << std::fixed << std::setprecision(2)
            << GetTotalPnL() << std::endl;

  if (total_cost_basis_ != 0.0) {
    double return_pct = (GetTotalPnL() / total_cost_basis_) * 100.0;
    std::cout << "Return on Equity: " << std::fixed << std::setprecision(2)
              << return_pct << "%" << std::endl;
  }

  std::cout << "Total Trades: " << total_trades_ << std::endl;

  // Add risk metrics
  auto risk_metrics = CalculateRiskMetrics();
  std::cout << "\n--- Risk Metrics ---" << std::endl;
  std::cout << "Max Position Value: $" << std::fixed << std::setprecision(2)
            << risk_metrics.max_position_value << std::endl;
  std::cout << "Volatility: " << std::fixed << std::setprecision(4)
            << risk_metrics.volatility << std::endl;
  std::cout << "Sharpe Ratio: " << std::fixed << std::setprecision(4)
            << risk_metrics.sharpe_ratio << std::endl;
  std::cout << "Max Drawdown: $" << std::fixed << std::setprecision(2)
            << risk_metrics.max_drawdown << std::endl;
  std::cout << "VaR 95%: " << std::fixed << std::setprecision(4)
            << risk_metrics.var_95 << std::endl;

  // Add performance statistics
  auto perf_stats = GetPerformanceStats();
  std::cout << "\n--- Performance Statistics ---" << std::endl;
  std::cout << "Win Rate: " << std::fixed << std::setprecision(2)
            << (perf_stats.win_rate * 100.0) << "%" << std::endl;
  std::cout << "Winning Trades: " << perf_stats.winning_trades << std::endl;
  std::cout << "Losing Trades: " << perf_stats.losing_trades << std::endl;
  std::cout << "Average Win: $" << std::fixed << std::setprecision(2)
            << perf_stats.avg_win << std::endl;
  std::cout << "Average Loss: $" << std::fixed << std::setprecision(2)
            << perf_stats.avg_loss << std::endl;
  std::cout << "Profit Factor: " << std::fixed << std::setprecision(2)
            << perf_stats.profit_factor << std::endl;
  std::cout << "Largest Win: $" << std::fixed << std::setprecision(2)
            << perf_stats.largest_win << std::endl;
  std::cout << "Largest Loss: $" << std::fixed << std::setprecision(2)
            << perf_stats.largest_loss << std::endl;
  if (csv_enabled_) {
    std::cout << "CSV Output: " << csv_filename_ << " (" << snapshots_.size()
              << " snapshots)" << std::endl;
    if (periodic_snapshots_enabled_) {
      std::cout << "Periodic Snapshots: Enabled (interval: "
                << (snapshot_interval_ns_ / 1000000) << "ms)" << std::endl;
    }
  }

  if (!tracked_order_ids_.empty()) {
    std::cout << "\n--- Tracked Order IDs ---" << std::endl;
    for (uint64_t order_id : tracked_order_ids_) {
      auto it = tracked_orders_.find(order_id);
      if (it != tracked_orders_.end()) {
        const auto &order = it->second;
        std::cout << "  Order " << order_id << ": "
                  << (order.is_buy ? "BUY" : "SELL") << " " << order.quantity
                  << " @ " << order.price
                  << " (remaining: " << order.remaining_quantity << ")"
                  << std::endl;
      }
    }
  }

  std::cout << "=============================================" << std::endl;
}

void PortfolioManager::EnableCSV(const std::string &filename) {
  if (csv_file_.is_open()) {
    csv_file_.close();
  }

  csv_filename_ = filename;
  csv_enabled_ = !filename.empty();

  if (csv_enabled_) {
    csv_file_.open(csv_filename_, std::ios::out | std::ios::trunc);
    if (csv_file_.is_open()) {
      csv_file_ << "# Portfolio Backtesting CSV Output\n";
      csv_file_ << "# Generated by PortfolioManager for user "
                << TRACKED_USER_ID << "\n";
      csv_file_ << "# Columns:\n";
      csv_file_ << "#   timestamp: ISO 8601 timestamp "
                   "(YYYY-MM-DDTHH:MM:SS.nnnnnnnnnZ)\n";
      csv_file_ << "#   position: Net position (positive=long, negative=short, "
                   "0=flat)\n";
      csv_file_ << "#   current_price: Latest market price in dollars "
                   "(converted from ticks)\n";
      csv_file_ << "#   average_cost: Average cost basis per unit in dollars\n";
      csv_file_ << "#   unrealized_pnl: Mark-to-market P&L for current "
                   "position in dollars\n";
      csv_file_ << "#   realized_pnl: Cumulative realized P&L from closed "
                   "trades in dollars\n";
      csv_file_
          << "#   total_pnl: Total P&L (realized + unrealized) in dollars\n";
      csv_file_ << "#   total_trades: Number of trade executions involving "
                   "tracked orders\n";
      csv_file_ << "#   total_cost_basis: Total cost basis for current "
                   "position in dollars\n";
      csv_file_ << "#   position_value: Current market value of position in "
                   "dollars\n";
      csv_file_
          << "#   return_on_equity: Total P&L as decimal (e.g., 0.15 = 15%)\n";
      csv_file_ << "\n";

      WriteCSVHeader();
      std::cout << "[PORTFOLIO] CSV logging enabled: " << csv_filename_
                << std::endl;
    } else {
      csv_enabled_ = false;
      std::cout << "[PORTFOLIO] Failed to open CSV file: " << csv_filename_
                << std::endl;
    }
  }
}

void PortfolioManager::DisableCSV() {
  if (csv_file_.is_open()) {
    csv_file_.close();
  }
  csv_enabled_ = false;
  std::cout << "[PORTFOLIO] CSV logging disabled" << std::endl;
}

// Private helper methods
void PortfolioManager::WriteCSVHeader() {
  if (!csv_enabled_ || !csv_file_.is_open()) {
    return;
  }

  csv_file_ << "timestamp,position,current_price,average_cost,unrealized_pnl,"
               "realized_pnl,total_pnl,total_trades,total_cost_basis,"
               "position_value,return_on_equity\n";
  csv_file_.flush();
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

void PortfolioManager::WriteSnapshotToCSV(const PortfolioSnapshot &snapshot) {
  if (!csv_enabled_ || !csv_file_.is_open()) {
    return;
  }

  std::string timestamp_str = TimestampToString(snapshot.timestamp);

  double current_price_dollars = snapshot.current_price / 100.0;
  double average_cost_dollars = snapshot.average_cost / 100.0;
  double unrealized_pnl_dollars = snapshot.unrealized_pnl / 100.0;
  double realized_pnl_dollars = snapshot.realized_pnl / 100.0;
  double total_pnl_dollars = snapshot.total_pnl / 100.0;
  double total_cost_basis_dollars = snapshot.total_cost_basis / 100.0;
  double position_value_dollars = snapshot.position_value / 100.0;

  csv_file_ << timestamp_str << "," << snapshot.position << "," << std::fixed
            << std::setprecision(2) << current_price_dollars << ","
            << std::fixed << std::setprecision(2) << average_cost_dollars << ","
            << std::fixed << std::setprecision(2) << unrealized_pnl_dollars
            << "," << std::fixed << std::setprecision(2) << realized_pnl_dollars
            << "," << std::fixed << std::setprecision(2) << total_pnl_dollars
            << "," << snapshot.total_trades << "," << std::fixed
            << std::setprecision(2) << total_cost_basis_dollars << ","
            << std::fixed << std::setprecision(2) << position_value_dollars
            << "," << std::fixed << std::setprecision(6)
            << snapshot.return_on_equity << "\n";

  static int write_count = 0;
  if (++write_count % 5 == 0) {
    csv_file_.flush();
  }
}

double PortfolioManager::CalculateAverageCost() const {
  if (running_position_ == 0 || total_cost_basis_ == 0.0) {
    return 0.0;
  }
  return total_cost_basis_ / static_cast<double>(std::abs(running_position_));
}

double PortfolioManager::CalculateUnrealizedPnL() const {
  if (running_position_ == 0 || current_market_price_ == 0.0) {
    return 0.0;
  }

  double avg_cost = CalculateAverageCost();
  if (avg_cost == 0.0) {
    return 0.0;
  }

  if (running_position_ > 0) {
    // Long position
    return static_cast<double>(running_position_) *
           (current_market_price_ - avg_cost);
  } else {
    // Short position
    return static_cast<double>(std::abs(running_position_)) *
           (avg_cost - current_market_price_);
  }
}

void PortfolioManager::TakeSnapshot(uint64_t timestamp) {
  if (timestamp == 0) {
    timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count();
  }

  PortfolioSnapshot snapshot(timestamp, running_position_,
                             current_market_price_, CalculateAverageCost(),
                             CalculateUnrealizedPnL(), realized_pnl_,
                             total_trades_, total_cost_basis_);

  snapshots_.push_back(snapshot);

  if (csv_enabled_) {
    WriteSnapshotToCSV(snapshot);
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
            << (interval_ns / 1000000) << "ms)" << std::endl;
}

void PortfolioManager::DisablePeriodicSnapshots() {
  periodic_snapshots_enabled_ = false;
  snapshot_interval_ns_ = 0;
  std::cout << "[PORTFOLIO] Periodic snapshots disabled" << std::endl;
}

void PortfolioManager::SetStrategyManager(
    std::shared_ptr<StrategyManager> strategy_mgr) {
  strategy_manager_ = strategy_mgr;
  std::cout << "[PORTFOLIO] Strategy manager set" << std::endl;
}

void PortfolioManager::SetUserStrategy(std::shared_ptr<Strategy> strategy) {
  if (strategy && strategy->GetUserId() != TRACKED_USER_ID) {
    std::cout << "[PORTFOLIO] Warning: Strategy user ID ("
              << strategy->GetUserId() << ") does not match tracked user ID ("
              << TRACKED_USER_ID << ")" << std::endl;
  }

  user_strategy_ = strategy;

  if (strategy) {
    std::cout << "[PORTFOLIO] User strategy set: " << strategy->GetName()
              << " for user " << strategy->GetUserId() << std::endl;

    // Set this portfolio manager in the strategy
    strategy->SetPortfolioManager(shared_from_this());

    // Add to strategy manager if available
    if (strategy_manager_) {
      strategy_manager_->AddStrategy(strategy->GetUserId(), strategy);
    }
  } else {
    std::cout << "[PORTFOLIO] User strategy cleared" << std::endl;

    // Remove from strategy manager if available
    if (strategy_manager_) {
      strategy_manager_->RemoveStrategy(TRACKED_USER_ID);
    }
  }
}

void PortfolioManager::Reset() {
  tracked_order_ids_.clear();
  tracked_orders_.clear();
  running_position_ = 0;
  realized_pnl_ = 0.0;
  total_cost_basis_ = 0.0;
  current_market_price_ = 0.0;
  total_trades_ = 0;
  snapshots_.clear();
  last_snapshot_timestamp_ = 0;

  std::cout << "[PORTFOLIO] Portfolio state reset for user " << TRACKED_USER_ID
            << std::endl;

  // Rewrite CSV header if CSV is enabled
  if (csv_enabled_ && csv_file_.is_open()) {
    csv_file_.close();
    EnableCSV(csv_filename_);
  }
}

void PortfolioManager::OnOrderCancelled(uint64_t order_id) {
  auto it = tracked_order_ids_.find(order_id);
  if (it != tracked_order_ids_.end()) {
    tracked_order_ids_.erase(it);
    tracked_orders_.erase(order_id);

    std::cout << "[PORTFOLIO] Order " << order_id
              << " cancelled and removed from tracking" << std::endl;
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
              << new_quantity << "@" << new_price << std::endl;
  }
}

const TrackedOrder *PortfolioManager::GetOrderDetails(uint64_t order_id) const {
  auto it = tracked_orders_.find(order_id);
  return (it != tracked_orders_.end()) ? &it->second : nullptr;
}

RiskMetrics PortfolioManager::CalculateRiskMetrics() const {
  RiskMetrics metrics{};

  if (snapshots_.empty()) {
    return metrics;
  }

  for (const auto &snapshot : snapshots_) {
    double pos_value = std::abs(snapshot.position_value);
    metrics.max_position_value =
        std::max(metrics.max_position_value, pos_value);
  }

  std::vector<double> pnl_series;
  std::vector<double> returns;

  for (const auto &snapshot : snapshots_) {
    pnl_series.push_back(snapshot.total_pnl);
  }

  if (pnl_series.size() > 1) {
    for (size_t i = 1; i < pnl_series.size(); ++i) {
      if (std::abs(pnl_series[i - 1]) > 1e-6) {
        double ret =
            (pnl_series[i] - pnl_series[i - 1]) / std::abs(pnl_series[i - 1]);
        returns.push_back(ret);
      }
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
      variance /= returns.size();
      metrics.volatility = std::sqrt(variance);

      if (metrics.volatility > 1e-6) {
        metrics.sharpe_ratio = mean_return / metrics.volatility;
      }
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
  }

  return metrics;
}

bool PortfolioManager::ExportData(const std::string &format,
                                  const std::string &filename) const {
  if (format == "csv") {
    if (snapshots_.empty()) {
      std::cout << "[PORTFOLIO] No data to export" << std::endl;
      return false;
    }

    std::ofstream export_file(filename);
    if (!export_file.is_open()) {
      std::cout << "[PORTFOLIO] Failed to open export file: " << filename
                << std::endl;
      return false;
    }

    export_file
        << "timestamp,position,current_price,average_cost,unrealized_pnl,"
           "realized_pnl,total_pnl,total_trades,total_cost_basis,position_"
           "value,return_on_equity\n";

    for (const auto &snapshot : snapshots_) {
      std::string timestamp_str = TimestampToString(snapshot.timestamp);
      export_file << timestamp_str << "," << snapshot.position << ","
                  << std::fixed << std::setprecision(2)
                  << snapshot.current_price << "," << snapshot.average_cost
                  << "," << snapshot.unrealized_pnl << ","
                  << snapshot.realized_pnl << "," << snapshot.total_pnl << ","
                  << snapshot.total_trades << "," << snapshot.total_cost_basis
                  << "," << snapshot.position_value << ","
                  << std::setprecision(6) << snapshot.return_on_equity << "\n";
    }

    export_file.close();
    std::cout << "[PORTFOLIO] Data exported to: " << filename << std::endl;
    return true;

  } else if (format == "json") {
    std::ofstream export_file(filename);
    if (!export_file.is_open()) {
      std::cout << "[PORTFOLIO] Failed to open export file: " << filename
                << std::endl;
      return false;
    }

    export_file << "{\n";
    export_file << "  \"user_id\": " << TRACKED_USER_ID << ",\n";
    export_file << "  \"summary\": {\n";
    export_file << "    \"total_trades\": " << total_trades_ << ",\n";
    export_file << "    \"running_position\": " << running_position_ << ",\n";
    export_file << "    \"realized_pnl\": " << realized_pnl_ << ",\n";
    export_file << "    \"unrealized_pnl\": " << CalculateUnrealizedPnL()
                << ",\n";
    export_file << "    \"total_pnl\": " << GetTotalPnL() << ",\n";
    export_file << "    \"current_price\": " << current_market_price_ << "\n";
    export_file << "  },\n";

    auto risk_metrics = CalculateRiskMetrics();
    export_file << "  \"risk_metrics\": {\n";
    export_file << "    \"max_position_value\": "
                << risk_metrics.max_position_value << ",\n";
    export_file << "    \"volatility\": " << risk_metrics.volatility << ",\n";
    export_file << "    \"sharpe_ratio\": " << risk_metrics.sharpe_ratio
                << ",\n";
    export_file << "    \"max_drawdown\": " << risk_metrics.max_drawdown
                << ",\n";
    export_file << "    \"var_95\": " << risk_metrics.var_95 << "\n";
    export_file << "  },\n";

    export_file << "  \"snapshots\": [\n";
    for (size_t i = 0; i < snapshots_.size(); ++i) {
      const auto &snapshot = snapshots_[i];
      export_file << "    {\n";
      export_file << "      \"timestamp\": \""
                  << TimestampToString(snapshot.timestamp) << "\",\n";
      export_file << "      \"position\": " << snapshot.position << ",\n";
      export_file << "      \"current_price\": " << snapshot.current_price
                  << ",\n";
      export_file << "      \"total_pnl\": " << snapshot.total_pnl << "\n";
      export_file << "    }";
      if (i < snapshots_.size() - 1) {
        export_file << ",";
      }
      export_file << "\n";
    }
    export_file << "  ]\n";
    export_file << "}\n";

    export_file.close();
    std::cout << "[PORTFOLIO] Data exported to JSON: " << filename << std::endl;
    return true;
  }

  std::cout << "[PORTFOLIO] Unsupported export format: " << format << std::endl;
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