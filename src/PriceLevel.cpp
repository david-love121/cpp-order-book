#include "PriceLevel.h"
#include "Order.h"
#include <stdexcept>
#include "Trade.h"
#include <algorithm>
#include "Helpers.h"
void PriceLevel::AddOrder(Order* order) {
    order_queue_.push_back(order);
    total_volume_ += order->quantity;
    order->parent_price_level = this;
    order->position_in_list = --order_queue_.end(); 
}
void PriceLevel::RemoveOrder(Order* order) {
    if (order->position_in_list != order_queue_.end()) {
        total_volume_ -= order->quantity;
        order_queue_.erase(order->position_in_list);
        order->parent_price_level = nullptr; // Clear the parent pointer
        order->position_in_list = order_queue_.end(); // Reset the iterator
    } else {
        throw std::runtime_error("Order not found in PriceLevel");
    }
}
std::vector<Trade> PriceLevel::FillOrder(Order* order, uint64_t quantity) {
    std::vector<Trade> trades;
    if (quantity == 0) {
        return trades; // No orders to fill or zero quantity
    }

    while (quantity > 0) {
        Order* top_order = GetTopOrder();
        uint64_t fill_quantity = std::min(quantity, top_order->quantity);
        Trade trade{
            order->order_id, // aggressor_order_id
            top_order->order_id, // resting_order_id
            order->user_id, // aggressor_user_id
            top_order->user_id, // resting_user_id
            price_, // price
            fill_quantity, // quantity
            order->timestamp, // ts_received
            Helpers::GetTimeStamp() // ts_executed
        };
        trades.push_back(trade);
        top_order->quantity -= fill_quantity;
        quantity -= fill_quantity;
        if (top_order->quantity == 0) {
            RemoveOrder(top_order); // Remove the order if fully filled
        }
    }

    return trades;
}