#include "PriceLevel.h"
#include "Order.h"
#include <stdexcept>
#include "Trade.h"
#include <algorithm>
#include "Helpers.h"
void PriceLevel::AddOrder(Order* order) {
    // Check for null pointer
    if (!order) {
        throw std::invalid_argument("Cannot add null order");
    }
    
    // Initialize price if this is the first order
    if (order_queue_.empty()) {
        price_ = order->price;
    }
    
    order_queue_.push_back(order);
    total_volume_ += order->quantity;
    order->parent_price_level = this;
    order->position_in_list = --order_queue_.end(); 
}
void PriceLevel::RemoveOrder(Order* order) {
    if (!order) {
        throw std::invalid_argument("Cannot remove null order");
    }
    
    // Check if the order belongs to this price level
    if (order->parent_price_level != this) {
        throw std::runtime_error("Order not found in PriceLevel");
    }
    
    // Find and remove the order
    auto it = std::find(order_queue_.begin(), order_queue_.end(), order);
    if (it != order_queue_.end()) {
        total_volume_ -= order->quantity;
        order_queue_.erase(it);
        order->parent_price_level = nullptr; // Clear the parent pointer
        order->position_in_list = order_queue_.end(); // Reset the iterator
    } else {
        throw std::runtime_error("Order not found in PriceLevel");
    }
}
std::vector<Trade> PriceLevel::FillOrder(Order* order, uint64_t quantity) {
    std::vector<Trade> trades;
    if (quantity == 0 || order_queue_.empty()) {
        return trades; // No orders to fill or zero quantity
    }

    uint64_t remaining_quantity = quantity;
    while (remaining_quantity > 0 && !order_queue_.empty()) {
        Order* top_order = GetTopOrder();
        if (!top_order) break;
        
        uint64_t fill_quantity = std::min(remaining_quantity, top_order->quantity);
        Trade trade{
            Helpers::GenerateExecutionId(), // execution_id
            order->order_id, // aggressor_order_id
            top_order->order_id, // resting_order_id
            order->user_id, // aggressor_user_id
            top_order->user_id, // resting_user_id
            price_, // price
            fill_quantity, // quantity
            order->ts_received, // ts_received (from aggressor order)
            order->ts_executed  // ts_executed (from aggressor order - should be historical timestamp)
        };
        trades.push_back(trade);
        
        // Update quantities
        top_order->quantity -= fill_quantity;
        total_volume_ -= fill_quantity;  // Update total volume
        remaining_quantity -= fill_quantity;
        
        if (top_order->quantity == 0) {
            // Remove the order from the price level - OrderBook will handle deletion
            order_queue_.pop_front();
            top_order->parent_price_level = nullptr;
        }
    }

    return trades;
}