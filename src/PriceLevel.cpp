#include "PriceLevel.h"
#include "Order.h"
#include <stdexcept>
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
