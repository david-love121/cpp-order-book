#include "OrderBook.h"
#include "Order.h"
#include "PriceLevel.h"
#include "Trade.h"
#include "Helpers.h"
#include <iostream>
#include <chrono>
#include <algorithm>

// Simplified AddOrder logic for illustration
void OrderBook::AddOrder(uint64_t order_id, uint64_t user_id, bool is_buy, uint64_t quantity, uint64_t price) {
    // 1. Check for existing order
    if (order_map_.count(order_id)) {
        // Handle error: duplicate order ID
        throw std::runtime_error("Order ID already exists");
        return;
    }

    // 2. Create new order object (from a memory pool in a real implementation)
    // Get current Unix timestamp in microseconds for high precision
    uint64_t timestamp = Helpers::GetTimeStamp();
    Order* new_order = new Order{order_id, user_id, is_buy, quantity, price, timestamp};

    // 3. Match against the book
    MatchOrders(new_order);

    // 4. If order has remaining quantity, add it as a resting order
    if (new_order->quantity > 0) {
        AddRestingOrder(new_order);
    } else {
        // Order fully filled, release memory
        delete new_order;
    }
}

// Simplified cancellation logic
void OrderBook::CancelOrder(uint64_t order_id) {
    auto it = order_map_.find(order_id);
    if (it == order_map_.end()) {
        throw std::runtime_error("Order ID not found");
        return;
    }

    Order* order_to_cancel = it->second;

    // O(1) removal from the PriceLevel's list
    order_to_cancel->parent_price_level->RemoveOrder(order_to_cancel);

    // O(1) removal from the main map
    order_map_.erase(it);

    // Release memory (back to the pool)
    delete order_to_cancel;
}
std::vector<Trade> OrderBook::MatchOrders(Order* incoming_order) {
    std::vector<Trade> executed_trades;
    
    if (incoming_order->is_buy_side) {
        // Buy order: match against asks (sell orders), starting from lowest price
        auto ask_it = asks_.begin();
        while (ask_it != asks_.end() && incoming_order->quantity > 0) {
            uint64_t ask_price = ask_it->first;
            PriceLevel& price_level = ask_it->second;
            
            // Check if the buy order price is >= ask price (can trade)
            if (incoming_order->price >= ask_price) {
                // Fill as much as possible at this price level
                uint64_t quantity_to_fill = std::min(incoming_order->quantity, price_level.GetTotalVolume());
                
                // Get trades from this price level
                std::vector<Trade> level_trades = price_level.FillOrder(incoming_order, quantity_to_fill);
                
                // Add trades to our result
                executed_trades.insert(executed_trades.end(), level_trades.begin(), level_trades.end());
                
                // Reduce incoming order quantity
                incoming_order->quantity -= quantity_to_fill;
                
                // If price level is empty, remove it
                if (price_level.GetTotalVolume() == 0) {
                    ask_it = asks_.erase(ask_it);
                } else {
                    ++ask_it;
                }
            } else {
                // Price doesn't match, stop matching
                break;
            }
        }
    } else {
        // Sell order: match against bids (buy orders), starting from highest price
        auto bid_it = bids_.rbegin(); // Reverse iterator to start from highest price
        while (bid_it != bids_.rend() && incoming_order->quantity > 0) {
            uint64_t bid_price = bid_it->first;
            PriceLevel& price_level = bid_it->second;
            
            // Check if the sell order price is <= bid price (can trade)
            if (incoming_order->price <= bid_price) {
                // Fill as much as possible at this price level
                uint64_t quantity_to_fill = std::min(incoming_order->quantity, price_level.GetTotalVolume());
                
                // Get trades from this price level
                std::vector<Trade> level_trades = price_level.FillOrder(incoming_order, quantity_to_fill);
                
                // Add trades to our result
                executed_trades.insert(executed_trades.end(), level_trades.begin(), level_trades.end());
                
                // Reduce incoming order quantity
                incoming_order->quantity -= quantity_to_fill;
                
                // If price level is empty, remove it
                if (price_level.GetTotalVolume() == 0) {
                    // Convert reverse iterator to regular iterator for erase
                    auto forward_it = std::next(bid_it).base();
                    bid_it = std::make_reverse_iterator(bids_.erase(forward_it));
                } else {
                    ++bid_it;
                }
            } else {
                // Price doesn't match, stop matching
                break;
            }
        }
    }
    
    return executed_trades;
}


uint64_t OrderBook::GetBestAsk() const {
    if (asks_.empty()) {
        return 0; // No asks available
    }
    return asks_.begin()->first; // Return the lowest ask price
}
uint64_t OrderBook::GetBestBid() const {
    if (bids_.empty()) {
        return 0; // No bids available
    }
    return bids_.begin()->first; // Return the highest bid price
}
uint64_t OrderBook::GetTotalAskVolume() const {
    uint64_t total_volume = 0;
    for (const auto& ask : asks_) {
        total_volume += ask.second.GetTotalVolume(); 
    }
    return total_volume;
}
uint64_t OrderBook::GetTotalBidVolume() const {
    uint64_t total_volume = 0;
    for (const auto& bid : bids_) {
        total_volume += bid.second.GetTotalVolume(); 
    }
    return total_volume;
}