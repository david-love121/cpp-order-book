#include "PortfolioManager.h"
#include "Trade.h"
#include <chrono>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <ctime>

PortfolioManager::PortfolioManager(const std::string& csv_filename) 
    : csv_filename_(csv_filename), csv_enabled_(!csv_filename.empty()) {
    
    std::cout << "[PORTFOLIO] Initialized for user " << TRACKED_USER_ID 
              << " (order ID tracking mode)" << std::endl;
    
    if (csv_enabled_) {
        EnableCSV(csv_filename);
    }
}

PortfolioManager::~PortfolioManager() {
    if (csv_file_.is_open()) {
        csv_file_.close();
    }
}

void PortfolioManager::OnOrderSubmitted(uint64_t order_id, uint64_t user_id, bool is_buy, 
                                        uint64_t quantity, uint64_t price, uint64_t timestamp) {
    // Only track orders from the target user
    if (user_id != TRACKED_USER_ID) {
        return;
    }
    
    // Use current time if timestamp not provided
    if (timestamp == 0) {
        timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    
    // Record the order ID and details
    tracked_order_ids_.insert(order_id);
    tracked_orders_.emplace(order_id, TrackedOrder(order_id, is_buy, quantity, price, timestamp));
    
    std::cout << "[PORTFOLIO] Tracking order " << order_id << " for user " << user_id 
              << " (" << (is_buy ? "BUY" : "SELL") << " " << quantity 
              << " @ " << price << ")" << std::endl;
}

void PortfolioManager::OnTradeExecuted(const Trade& trade) {
    // Check if either the aggressor or resting order belongs to our tracked user
    bool aggressor_tracked = tracked_order_ids_.count(trade.aggressor_order_id) > 0;
    bool resting_tracked = tracked_order_ids_.count(trade.resting_order_id) > 0;
    
    if (!aggressor_tracked && !resting_tracked) {
        // No tracked orders involved in this trade, but update market price
        current_market_price_ = static_cast<double>(trade.price);
        return;
    }
    
    // Update market price with trade price
    current_market_price_ = static_cast<double>(trade.price);
    
    // Determine position change and cost basis impact
    int64_t position_change = 0;
    double trade_cost = 0.0;
    
    if (aggressor_tracked) {
        // Our user was the aggressor
        auto it = tracked_orders_.find(trade.aggressor_order_id);
        if (it != tracked_orders_.end()) {
            // Aggressor: BUY increases position, SELL decreases position
            int64_t qty_change = it->second.is_buy ? 
                static_cast<int64_t>(trade.quantity) : -static_cast<int64_t>(trade.quantity);
            position_change += qty_change;
            
            // Update remaining quantity for the order
            it->second.remaining_quantity -= trade.quantity;
            
            // Calculate cost impact
            if (it->second.is_buy) {
                trade_cost += static_cast<double>(trade.quantity * trade.price);
            } else {
                trade_cost -= static_cast<double>(trade.quantity * trade.price);
            }
        }
    }
    
    if (resting_tracked) {
        // Our user had the resting order
        auto it = tracked_orders_.find(trade.resting_order_id);
        if (it != tracked_orders_.end()) {
            // Resting: BUY increases position, SELL decreases position
            int64_t qty_change = it->second.is_buy ? 
                static_cast<int64_t>(trade.quantity) : -static_cast<int64_t>(trade.quantity);
            position_change += qty_change;
            
            // Update remaining quantity for the order
            it->second.remaining_quantity -= trade.quantity;
            
            // Calculate cost impact
            if (it->second.is_buy) {
                trade_cost += static_cast<double>(trade.quantity * trade.price);
            } else {
                trade_cost -= static_cast<double>(trade.quantity * trade.price);
            }
        }
    }
    
    // Calculate realized P&L if we're reducing position
    if ((running_position_ > 0 && position_change < 0) || (running_position_ < 0 && position_change > 0)) {
        // We're closing some position - calculate realized P&L
        double avg_cost = CalculateAverageCost();
        double realized_qty = std::min(static_cast<double>(std::abs(position_change)), 
                                     static_cast<double>(std::abs(running_position_)));
        
        if (running_position_ > 0) {
            // Closing long position
            realized_pnl_ += realized_qty * (current_market_price_ - avg_cost);
        } else {
            // Closing short position
            realized_pnl_ += realized_qty * (avg_cost - current_market_price_);
        }
    }
    
    // Update position and cost basis
    int64_t old_position = running_position_;
    running_position_ += position_change;
    
    // Update cost basis
    if (position_change > 0) {
        // Adding to position
        total_cost_basis_ += std::abs(trade_cost);
    } else if (position_change < 0) {
        // Reducing position - adjust cost basis proportionally
        if (old_position != 0) {
            double reduction_ratio = static_cast<double>(std::abs(position_change)) / static_cast<double>(std::abs(old_position));
            total_cost_basis_ *= (1.0 - reduction_ratio);
        }
    }
    
    // If position is now zero, reset cost basis
    if (running_position_ == 0) {
        total_cost_basis_ = 0.0;
    }
    
    total_trades_++;
    
    std::cout << "[PORTFOLIO] Trade executed involving tracked order(s). Position change: " 
              << (position_change >= 0 ? "+" : "") << position_change 
              << ", Running position: " << running_position_ 
              << ", Price: " << current_market_price_ << std::endl;
    
    // Take a snapshot after each trade
    TakeSnapshot(trade.ts_executed);
}

void PortfolioManager::UpdateMarketPrice(double price, uint64_t timestamp) {
    current_market_price_ = price;
    
    if (timestamp == 0) {
        timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
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
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    TakeSnapshot(timestamp);
}

void PortfolioManager::PrintPortfolioSummary() const {
    std::cout << "\n=== Portfolio Summary (User " << TRACKED_USER_ID << ") ===" << std::endl;
    std::cout << "Tracked Orders: " << tracked_order_ids_.size() << std::endl;
    std::cout << "Running Position: " << running_position_ << " contracts" << std::endl;
    std::cout << "Current Market Price: $" << std::fixed << std::setprecision(2) << current_market_price_ << std::endl;
    std::cout << "Average Cost: $" << std::fixed << std::setprecision(2) << CalculateAverageCost() << std::endl;
    std::cout << "Total Cost Basis: $" << std::fixed << std::setprecision(2) << total_cost_basis_ << std::endl;
    std::cout << "Position Value: $" << std::fixed << std::setprecision(2) << (current_market_price_ * std::abs(running_position_)) << std::endl;
    std::cout << "Realized P&L: $" << std::fixed << std::setprecision(2) << realized_pnl_ << std::endl;
    std::cout << "Unrealized P&L: $" << std::fixed << std::setprecision(2) << CalculateUnrealizedPnL() << std::endl;
    std::cout << "Total P&L: $" << std::fixed << std::setprecision(2) << GetTotalPnL() << std::endl;
    
    if (total_cost_basis_ != 0.0) {
        double return_pct = (GetTotalPnL() / total_cost_basis_) * 100.0;
        std::cout << "Return on Equity: " << std::fixed << std::setprecision(2) << return_pct << "%" << std::endl;
    }
    
    std::cout << "Total Trades: " << total_trades_ << std::endl;
    
    if (csv_enabled_) {
        std::cout << "CSV Output: " << csv_filename_ << " (" << snapshots_.size() << " snapshots)" << std::endl;
        if (periodic_snapshots_enabled_) {
            std::cout << "Periodic Snapshots: Enabled (interval: " << (snapshot_interval_ns_ / 1000000) << "ms)" << std::endl;
        }
    }
    
    if (!tracked_order_ids_.empty()) {
        std::cout << "\n--- Tracked Order IDs ---" << std::endl;
        for (uint64_t order_id : tracked_order_ids_) {
            auto it = tracked_orders_.find(order_id);
            if (it != tracked_orders_.end()) {
                const auto& order = it->second;
                std::cout << "  Order " << order_id << ": " 
                         << (order.is_buy ? "BUY" : "SELL") << " " << order.quantity 
                         << " @ " << order.price << " (remaining: " << order.remaining_quantity << ")" << std::endl;
            }
        }
    }
    
    std::cout << "=============================================" << std::endl;
}

void PortfolioManager::EnableCSV(const std::string& filename) {
    if (csv_file_.is_open()) {
        csv_file_.close();
    }
    
    csv_filename_ = filename;
    csv_enabled_ = !filename.empty();
    
    if (csv_enabled_) {
        csv_file_.open(csv_filename_, std::ios::out | std::ios::trunc);
        if (csv_file_.is_open()) {
            // Write CSV documentation header
            csv_file_ << "# Portfolio Backtesting CSV Output\n";
            csv_file_ << "# Generated by PortfolioManager for user " << TRACKED_USER_ID << "\n";
            csv_file_ << "# Columns:\n";
            csv_file_ << "#   timestamp: ISO 8601 timestamp (YYYY-MM-DDTHH:MM:SS.nnnnnnnnnZ)\n";
            csv_file_ << "#   position: Net position (positive=long, negative=short, 0=flat)\n";
            csv_file_ << "#   current_price: Latest market price in dollars (converted from ticks)\n";
            csv_file_ << "#   average_cost: Average cost basis per unit in dollars\n";
            csv_file_ << "#   unrealized_pnl: Mark-to-market P&L for current position in dollars\n";
            csv_file_ << "#   realized_pnl: Cumulative realized P&L from closed trades in dollars\n";
            csv_file_ << "#   total_pnl: Total P&L (realized + unrealized) in dollars\n";
            csv_file_ << "#   total_trades: Number of trade executions involving tracked orders\n";
            csv_file_ << "#   total_cost_basis: Total cost basis for current position in dollars\n";
            csv_file_ << "#   position_value: Current market value of position in dollars\n";
            csv_file_ << "#   return_on_equity: Total P&L as decimal (e.g., 0.15 = 15%)\n";
            csv_file_ << "\n";
            
            WriteCSVHeader();
            std::cout << "[PORTFOLIO] CSV logging enabled: " << csv_filename_ << std::endl;
        } else {
            csv_enabled_ = false;
            std::cout << "[PORTFOLIO] Failed to open CSV file: " << csv_filename_ << std::endl;
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
    if (!csv_enabled_ || !csv_file_.is_open()) return;
    
    csv_file_ << "timestamp,position,current_price,average_cost,unrealized_pnl,realized_pnl,total_pnl,"
              << "total_trades,total_cost_basis,position_value,return_on_equity\n";
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
    oss << "." << std::setfill('0') << std::setw(9) << timestamp_nanos_remainder << "Z";
    
    return oss.str();
}

void PortfolioManager::WriteSnapshotToCSV(const PortfolioSnapshot& snapshot) {
    if (!csv_enabled_ || !csv_file_.is_open()) return;
    
    // Convert timestamp to readable string
    std::string timestamp_str = TimestampToString(snapshot.timestamp);
    
    // Convert prices from ticks to dollars (divide by 100)
    double current_price_dollars = snapshot.current_price / 100.0;
    double average_cost_dollars = snapshot.average_cost / 100.0;
    double unrealized_pnl_dollars = snapshot.unrealized_pnl / 100.0;
    double realized_pnl_dollars = snapshot.realized_pnl / 100.0;
    double total_pnl_dollars = snapshot.total_pnl / 100.0;
    double total_cost_basis_dollars = snapshot.total_cost_basis / 100.0;
    double position_value_dollars = snapshot.position_value / 100.0;
    
    csv_file_ << timestamp_str << ","
              << snapshot.position << ","
              << std::fixed << std::setprecision(2) << current_price_dollars << ","
              << std::fixed << std::setprecision(2) << average_cost_dollars << ","
              << std::fixed << std::setprecision(2) << unrealized_pnl_dollars << ","
              << std::fixed << std::setprecision(2) << realized_pnl_dollars << ","
              << std::fixed << std::setprecision(2) << total_pnl_dollars << ","
              << snapshot.total_trades << ","
              << std::fixed << std::setprecision(2) << total_cost_basis_dollars << ","
              << std::fixed << std::setprecision(2) << position_value_dollars << ","
              << std::fixed << std::setprecision(6) << snapshot.return_on_equity << "\n";
    csv_file_.flush();
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
        return static_cast<double>(running_position_) * (current_market_price_ - avg_cost);
    } else {
        // Short position
        return static_cast<double>(std::abs(running_position_)) * (avg_cost - current_market_price_);
    }
}

void PortfolioManager::TakeSnapshot(uint64_t timestamp) {
    if (timestamp == 0) {
        timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    
    PortfolioSnapshot snapshot(
        timestamp,
        running_position_,
        current_market_price_,
        CalculateAverageCost(),
        CalculateUnrealizedPnL(),
        realized_pnl_,
        total_trades_,
        total_cost_basis_
    );
    
    snapshots_.push_back(snapshot);
    
    if (csv_enabled_) {
        WriteSnapshotToCSV(snapshot);
    }
}

void PortfolioManager::EnablePeriodicSnapshots(uint64_t interval_ns) {
    snapshot_interval_ns_ = interval_ns;
    periodic_snapshots_enabled_ = true;
    last_snapshot_timestamp_ = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    std::cout << "[PORTFOLIO] Periodic snapshots enabled (interval: " 
              << (interval_ns / 1000000) << "ms)" << std::endl;
}

void PortfolioManager::DisablePeriodicSnapshots() {
    periodic_snapshots_enabled_ = false;
    snapshot_interval_ns_ = 0;
    std::cout << "[PORTFOLIO] Periodic snapshots disabled" << std::endl;
}
