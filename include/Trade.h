#pragma once
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>

struct Trade {
    uint64_t aggressor_order_id;
    uint64_t resting_order_id;
    uint64_t aggressor_user_id;
    uint64_t resting_user_id;
    uint64_t price;
    uint64_t quantity;
    uint64_t ts_received;
    uint64_t ts_executed;
    bool aggressor_order_closed;
    bool resting_order_closed;
    bool aggressor_is_buy;

    // Helper function to convert trade to string for logging
    std::string ToString() const {
        std::stringstream ss;
        ss << "Trade(aggressor_id=" << aggressor_order_id
           << ", resting_id=" << resting_order_id
           << ", aggressor_user=" << aggressor_user_id
           << ", resting_user=" << resting_user_id
           << ", side=" << (aggressor_is_buy ? "BUY" : "SELL")
           << ", price=" << std::fixed << std::setprecision(2) << (price / 100.0)
           << ", quantity=" << quantity
           << ", ts=" << ts_executed << ")";
        return ss.str();
    }
};