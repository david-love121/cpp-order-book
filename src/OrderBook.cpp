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
    // 1. Validate inputs
    if (quantity == 0) {
        throw std::invalid_argument("Order quantity must be greater than zero");
    }
    
    // 2. Check for existing order
    if (order_map_.count(order_id)) {
        // Handle error: duplicate order ID
        throw std::runtime_error("Order ID already exists");
        return;
    }

    // 3. Create new order object (from a memory pool in a real implementation)
    // Get current Unix timestamp in microseconds for high precision
    uint64_t timestamp = Helpers::GetTimeStamp();
    Order* new_order = new Order{order_id, user_id, is_buy, quantity, price, timestamp};

    // 4. Match against the book
    MatchOrders(new_order);

    // 5. If order has remaining quantity, add it as a resting order
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

    // Check if order was already filled (parent_price_level would be null)
    if (order_to_cancel->parent_price_level != nullptr) {
        // O(1) removal from the PriceLevel's list
        order_to_cancel->parent_price_level->RemoveOrder(order_to_cancel);
    }

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
                
                // Clean up any filled orders from order_map
                for (const auto& trade : level_trades) {
                    // Check if the resting order was fully filled
                    auto order_it = order_map_.find(trade.resting_order_id);
                    if (order_it != order_map_.end()) {
                        Order* resting_order = order_it->second;
                        if (resting_order->quantity == 0) {
                            // Order fully filled, remove from map and delete
                            order_map_.erase(order_it);
                            delete resting_order;
                        }
                    }
                }
                
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
        // bids_ is already sorted highest-to-lowest due to std::greater<uint64_t>
        auto bid_it = bids_.begin();
        while (bid_it != bids_.end() && incoming_order->quantity > 0) {
            uint64_t bid_price = bid_it->first;
            PriceLevel& price_level = bid_it->second;
            
            // Check if the sell order price is <= bid price (can trade)
            if (incoming_order->price <= bid_price) {
                // Fill as much as possible at this price level
                uint64_t quantity_to_fill = std::min(incoming_order->quantity, price_level.GetTotalVolume());
                
                // Get trades from this price level
                std::vector<Trade> level_trades = price_level.FillOrder(incoming_order, quantity_to_fill);
                
                // Clean up any filled orders from order_map
                for (const auto& trade : level_trades) {
                    // Check if the resting order was fully filled
                    auto order_it = order_map_.find(trade.resting_order_id);
                    if (order_it != order_map_.end()) {
                        Order* resting_order = order_it->second;
                        if (resting_order->quantity == 0) {
                            // Order fully filled, remove from map and delete
                            order_map_.erase(order_it);
                            delete resting_order;
                        }
                    }
                }
                
                // Add trades to our result
                executed_trades.insert(executed_trades.end(), level_trades.begin(), level_trades.end());
                
                // Reduce incoming order quantity
                incoming_order->quantity -= quantity_to_fill;
                
                // If price level is empty, remove it
                if (price_level.GetTotalVolume() == 0) {
                    bid_it = bids_.erase(bid_it);
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

void OrderBook::AddRestingOrder(Order* order) {
    order_map_[order->order_id] = order;
    
    if (order->is_buy_side) {
        // Add to bids
        auto& price_level = bids_[order->price];
        price_level.AddOrder(order);
    } else {
        // Add to asks  
        auto& price_level = asks_[order->price];
        price_level.AddOrder(order);
    }
}

void OrderBook::RemoveRestingOrder(Order* order) {
    // Remove from order map
    order_map_.erase(order->order_id);
    
    // Remove from price level
    if (order->parent_price_level) {
        order->parent_price_level->RemoveOrder(order);
        
        // If price level is now empty, remove it from the map
        if (order->parent_price_level->GetTotalVolume() == 0) {
            if (order->is_buy_side) {
                bids_.erase(order->price);
            } else {
                asks_.erase(order->price);
            }
        }
    }
}

// Order modification logic
void OrderBook::ModifyOrder(uint64_t order_id, uint64_t new_quantity, uint64_t new_price) {
    // 1. Validate inputs
    if (new_quantity == 0) {
        throw std::invalid_argument("Modified order quantity must be greater than zero");
    }
    
    // 2. Find the existing order
    auto it = order_map_.find(order_id);
    if (it == order_map_.end()) {
        throw std::runtime_error("Order ID not found");
    }
    
    Order* existing_order = it->second;
    
    // 3. Check if order was already filled (parent_price_level would be null)
    if (existing_order->parent_price_level == nullptr) {
        throw std::runtime_error("Cannot modify filled order");
    }
    
    // 4. Store original values
    uint64_t original_quantity = existing_order->quantity;
    uint64_t original_price = existing_order->price;
    bool is_buy = existing_order->is_buy_side;
    uint64_t user_id = existing_order->user_id;
    uint64_t timestamp = existing_order->timestamp;
    
    // 5. Use cancel-and-replace approach for simplicity and correctness
    // This ensures proper time priority and matching logic
    
    // Store the price level reference before removing the order
    PriceLevel* original_price_level = existing_order->parent_price_level;
    
    // Remove the existing order from price level and order map
    original_price_level->RemoveOrder(existing_order);
    order_map_.erase(it);
    
    // Clean up empty price level if necessary
    if (original_price_level->GetTotalVolume() == 0) {
        if (is_buy) {
            bids_.erase(original_price);
        } else {
            asks_.erase(original_price);
        }
    }
    
    // Delete the old order
    delete existing_order;
    
    // Create new order with modified parameters
    // For quantity reductions, keep original timestamp to preserve time priority
    // For other changes, use new timestamp
    uint64_t new_timestamp = (new_price == original_price && new_quantity <= original_quantity) ? 
                             timestamp : Helpers::GetTimeStamp();
    
    Order* new_order = new Order{order_id, user_id, is_buy, new_quantity, new_price, new_timestamp};
    
    // Match against the book (this handles the matching logic properly)
    MatchOrders(new_order);
    
    // If order has remaining quantity, add it as a resting order
    if (new_order->quantity > 0) {
        AddRestingOrder(new_order);
    } else {
        // Order fully filled, release memory
        delete new_order;
    }
}

// OrderBook destructor - clean up all remaining orders
OrderBook::~OrderBook() {
    // Delete all remaining orders in the order map
    for (auto& pair : order_map_) {
        delete pair.second; // Delete the Order object
    }
    order_map_.clear();
    
    // Clear the price levels (they are value types, so this is automatic)
    bids_.clear();
    asks_.clear();
}