#include "OrderBook.h"
#include "Order.h"
#include <iostream>

// Simplified AddOrder logic for illustration
void OrderBook::AddOrder(uint64_t order_id, bool is_buy, uint64_t quantity, uint64_t price) {
    // 1. Check for existing order
    if (order_map_.count(order_id)) {
        // Handle error: duplicate order ID
        throw std::runtime_error("Order ID already exists");
        return;
    }

    // 2. Create new order object (from a memory pool in a real implementation)
    Order* new_order = new Order{order_id, is_buy, quantity, price, /* timestamp */};

    // 3. Match against the book
    if (is_buy) {
        MatchWithAsks(new_order->order_id);
    } else {
        MatchWithBids(new_order->order_id);
    }

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
        // Handle error: order not found
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