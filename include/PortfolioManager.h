#pragma once

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Forward declarations
struct Trade;
class IStrategy;


struct TrackedOrder {
  uint64_t order_id;
  bool is_buy;
  uint64_t quantity;
  uint64_t remaining_quantity;
  uint64_t price;
  uint64_t timestamp;

  TrackedOrder(uint64_t id, bool buy, uint64_t qty, uint64_t px, uint64_t ts)
      : order_id(id), is_buy(buy), quantity(qty), remaining_quantity(qty),
        price(px), timestamp(ts) {}
};

struct PortfolioSnapshot {
  uint64_t timestamp;
  int64_t position;
  double current_price;
  double average_cost;
  double unrealized_pnl;
  double realized_pnl;
  double total_pnl;
  size_t total_trades;
  double position_value;

  PortfolioSnapshot(uint64_t ts, int64_t pos, double cur_price, double avg_cost,
                    double unrealized, double realized, size_t trades)
      : timestamp(ts), position(pos), current_price(cur_price),
        average_cost(avg_cost), unrealized_pnl(unrealized),
        realized_pnl(realized), total_pnl(realized + unrealized),
        total_trades(trades),
        position_value(cur_price * std::abs(pos)) {}

};

struct RiskMetrics {
  double max_position_value = 0.0;
  double var_95 = 0.0;
  double sharpe_ratio = 0.0;
  double max_drawdown = 0.0;
  double volatility = 0.0;
};

struct PerformanceStats {
  double win_rate = 0.0;
  double avg_win = 0.0;
  double avg_loss = 0.0;
  double profit_factor = 0.0;
  size_t winning_trades = 0;
  size_t losing_trades = 0;
  double largest_win = 0.0;
  double largest_loss = 0.0;
};

class PortfolioManager : public std::enable_shared_from_this<PortfolioManager> {
public:
  uint64_t tracked_user_id_;

  explicit PortfolioManager(uint64_t tracked_user_id, const std::string &csv_filename = "");
  ~PortfolioManager();

  // Order tracking methods
  void OnOrderSubmitted(uint64_t order_id, uint64_t user_id, bool is_buy,
                        uint64_t quantity, uint64_t price,
                        uint64_t timestamp = 0);
  void OnOrderCancelled(uint64_t order_id);
  void OnOrderModified(uint64_t order_id, uint64_t new_quantity,
                       uint64_t new_price);
  void OnTrade(const Trade &trade);

  // Market data methods
  void UpdateMarketPrice(double price, uint64_t timestamp = 0);

  // Snapshot methods
  void ForceSnapshot(uint64_t timestamp = 0);
  void EnablePeriodicSnapshots(uint64_t interval_ns);
  void DisablePeriodicSnapshots();

  // CSV output methods
  void EnableCSV(const std::string &filename);
  void DisableCSV();

  // Strategy integration

  void SetUserStrategy(std::shared_ptr<IStrategy> strategy);

  // Utility methods
  void PrintPortfolioSummary() const;
  void Reset();
  
  // Risk and performance methods
  void SetStopLoss(double percentage) { stop_loss_percentage_ = percentage; }
  void SetMaxDailyLoss(double loss) { max_daily_loss_ = loss; }
  bool IsStrategyDisabled() const;
  RiskMetrics CalculateRiskMetrics() const;
  bool ExportData(const std::string &format, const std::string &filename) const;
  PerformanceStats GetPerformanceStats() const;

  // Getters
  int64_t GetRunningPosition() const { return running_position_; }
  double GetRealizedPnL() const { return realized_pnl_; }
  double GetUnrealizedPnL() const { return CalculateUnrealizedPnL(); }
  double GetTotalPnL() const {
    return realized_pnl_ + CalculateUnrealizedPnL();
  }
  double GetCurrentMarketPrice() const { return current_market_price_; }
  double GetTotalCostBasis() const;
  double GetPositionValue() const {
    return current_market_price_ * std::abs(running_position_);
  }
  double GetReturnOnEquity() const;
  size_t GetTotalTrades() const { return total_trades_; }
  bool IsOrderTracked(uint64_t order_id) const {
    return tracked_order_ids_.count(order_id) > 0;
  }
  const std::unordered_set<uint64_t> &GetTrackedOrderIds() const {
    return tracked_order_ids_;
  }
  const std::vector<PortfolioSnapshot> &GetSnapshots() const {
    return snapshots_;
  }
  const TrackedOrder *GetOrderDetails(uint64_t order_id) const;

private:
  // Core portfolio state
  std::unordered_set<uint64_t> tracked_order_ids_;
  std::unordered_map<uint64_t, TrackedOrder> tracked_orders_;
  int64_t running_position_ = 0;
  double realized_pnl_ = 0.0;
  double average_cost_ = 0.0;
  double current_market_price_ = 0.0;
  size_t total_trades_ = 0;

  // Risk management state
  double stop_loss_percentage_ = 0.0; // 0.0 means no stop-loss
  double max_daily_loss_ = 0.0;       // 0.0 means no daily loss limit
  bool strategy_disabled_ = false;

  // Snapshot management
  std::vector<PortfolioSnapshot> snapshots_;
  bool periodic_snapshots_enabled_ = false;
  uint64_t snapshot_interval_ns_ = 1000000000; // 1 second default
  uint64_t last_snapshot_timestamp_ = 0;

  // CSV output
  std::string csv_filename_;
  std::ofstream csv_file_;
  bool csv_enabled_ = false;




  // Helper methods
  void CheckStopLoss();
  void CheckDailyLossLimit();
  void HandleLongPositionTrade(bool is_buy, double trade_price,
                               int64_t trade_quantity);
  void HandleShortPositionTrade(bool is_buy, double trade_price,
                                int64_t trade_quantity);
  void TakeSnapshot(uint64_t timestamp = 0);
  void WriteCSVHeader();
  void WriteSnapshotToCSV(const PortfolioSnapshot &snapshot);
  std::string TimestampToString(uint64_t timestamp_ns) const;
  double CalculateUnrealizedPnL() const;
};
