#pragma once

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <cstdint>

// Forward declaration
struct Trade;

/**
 * @brief Order tracking information
 */
struct TrackedOrder {
    uint64_t order_id;
    bool is_buy;
    uint64_t quantity;
    uint64_t price;
    uint64_t timestamp;
    uint64_t remaining_quantity;  // Track partial fills
    
    TrackedOrder(uint64_t id, bool buy, uint64_t qty, uint64_t prc = 0, uint64_t ts = 0) 
        : order_id(id), is_buy(buy), quantity(qty), price(prc), timestamp(ts), remaining_quantity(qty) {}
};

/**
 * @brief Portfolio snapshot for CSV tracking
 */
struct PortfolioSnapshot {
    uint64_t timestamp;
    int64_t position;
    double current_price;
    double average_cost;
    double unrealized_pnl;
    double realized_pnl;
    double total_pnl;
    size_t total_trades;
    double total_cost_basis;
    double position_value;  // current_price * abs(position)
    double return_on_equity;  // total_pnl / total_cost_basis (if not zero)
    
    PortfolioSnapshot(uint64_t ts = 0, int64_t pos = 0, double curr_price = 0.0, 
                     double avg_cost = 0.0, double unrealized = 0.0, double realized = 0.0, 
                     size_t trades = 0, double cost_basis = 0.0)
        : timestamp(ts), position(pos), current_price(curr_price), average_cost(avg_cost),
          unrealized_pnl(unrealized), realized_pnl(realized), total_pnl(unrealized + realized), 
          total_trades(trades), total_cost_basis(cost_basis),
          position_value(curr_price * std::abs(pos)),
          return_on_equity(cost_basis != 0.0 ? (unrealized + realized) / cost_basis : 0.0) {}
};

/**
 * @brief Single-user portfolio manager for tracking order IDs and position
 * 
 * This class tracks only order IDs submitted by user_id=1000 and maintains
 * a running position based on trades involving those tracked orders.
 * It also tracks P&L and writes portfolio snapshots to CSV for backtesting analysis.
 */
class PortfolioManager {
private:
    // The single user ID being tracked (hardcoded to 1000)
    static constexpr uint64_t TRACKED_USER_ID = 1000;
    
    // Set of order IDs submitted by the tracked user
    std::unordered_set<uint64_t> tracked_order_ids_;
    
    // Order details for tracked orders (for position calculation)
    std::unordered_map<uint64_t, TrackedOrder> tracked_orders_;
    
    // Running position across all symbols (net position)
    int64_t running_position_ = 0;
    
    // P&L tracking
    double realized_pnl_ = 0.0;
    double total_cost_basis_ = 0.0;  // Total cost of current position
    double current_market_price_ = 0.0;  // Latest trade price seen
    size_t total_trades_ = 0;
    
    // CSV tracking
    std::string csv_filename_;
    std::ofstream csv_file_;
    std::vector<PortfolioSnapshot> snapshots_;
    bool csv_enabled_;
    
    // Periodic snapshot tracking
    uint64_t last_snapshot_timestamp_ = 0;
    uint64_t snapshot_interval_ns_ = 0;  // 0 = disabled
    bool periodic_snapshots_enabled_ = false;
    
    // Helper methods
    void WriteCSVHeader();
    void WriteSnapshotToCSV(const PortfolioSnapshot& snapshot);
    double CalculateAverageCost() const;
    double CalculateUnrealizedPnL() const;
    void TakeSnapshot(uint64_t timestamp);
    std::string TimestampToString(uint64_t timestamp_ns) const;
    
public:
    /**
     * @brief Constructor for single-user PortfolioManager
     * @param csv_filename Optional filename for CSV output (if empty, CSV is disabled)
     */
    PortfolioManager(const std::string& csv_filename = "");
    
    /**
     * @brief Destructor - ensure CSV file is properly closed
     */
    ~PortfolioManager();
    
    /**
     * @brief Get the tracked user ID
     * @return The user ID being tracked (always 1000)
     */
    uint64_t GetTrackedUserId() const { return TRACKED_USER_ID; }
    
    /**
     * @brief Check if a user ID is the tracked user
     * @param user_id User ID to check
     * @return true if this is the tracked user, false otherwise
     */
    bool IsUserTracked(uint64_t user_id) const { return user_id == TRACKED_USER_ID; }
    
    /**
     * @brief Handle order submission - track order ID if submitted by tracked user
     * @param order_id The order ID
     * @param user_id The user submitting the order
     * @param is_buy True for buy order, false for sell order
     * @param quantity Order quantity
     * @param price Order price
     * @param timestamp Order timestamp
     */
    void OnOrderSubmitted(uint64_t order_id, uint64_t user_id, bool is_buy, uint64_t quantity, 
                         uint64_t price = 0, uint64_t timestamp = 0);
    
    /**
     * @brief Handle trade execution - update position if tracked order is involved
     * @param trade Trade details
     */
    void OnTradeExecuted(const Trade& trade);
    
    /**
     * @brief Update market price for unrealized P&L calculations
     * @param price Current market price
     * @param timestamp Timestamp of price update
     */
    void UpdateMarketPrice(double price, uint64_t timestamp = 0);
    
    /**
     * @brief Force a portfolio snapshot (useful for periodic tracking)
     * @param timestamp Timestamp for the snapshot
     */
    void ForceSnapshot(uint64_t timestamp = 0);
    
    /**
     * @brief Print portfolio summary
     */
    void PrintPortfolioSummary() const;
    
    /**
     * @brief Enable/disable CSV output and set filename
     * @param filename CSV filename (empty to disable)
     */
    void EnableCSV(const std::string& filename);
    
    /**
     * @brief Disable CSV output
     */
    void DisableCSV();
    
    /**
     * @brief Take periodic snapshots for regular tracking during simulation
     * @param interval_ns Interval in nanoseconds between snapshots
     */
    void EnablePeriodicSnapshots(uint64_t interval_ns = 1000000000); // Default 1 second
    
    /**
     * @brief Disable periodic snapshots
     */
    void DisablePeriodicSnapshots();
    
    // Getters
    size_t GetTrackedOrderCount() const { return tracked_order_ids_.size(); }
    int64_t GetRunningPosition() const { return running_position_; }
    double GetRealizedPnL() const { return realized_pnl_; }
    double GetUnrealizedPnL() const { return CalculateUnrealizedPnL(); }
    double GetTotalPnL() const { return realized_pnl_ + CalculateUnrealizedPnL(); }
    double GetCurrentMarketPrice() const { return current_market_price_; }
    double GetAverageCost() const { return CalculateAverageCost(); }
    double GetTotalCostBasis() const { return total_cost_basis_; }
    double GetPositionValue() const { return current_market_price_ * std::abs(running_position_); }
    double GetReturnOnEquity() const { 
        return total_cost_basis_ != 0.0 ? GetTotalPnL() / total_cost_basis_ : 0.0; 
    }
    size_t GetTotalTrades() const { return total_trades_; }
    bool IsOrderTracked(uint64_t order_id) const { return tracked_order_ids_.count(order_id) > 0; }
    const std::unordered_set<uint64_t>& GetTrackedOrderIds() const { return tracked_order_ids_; }
    const std::vector<PortfolioSnapshot>& GetSnapshots() const { return snapshots_; }
};
