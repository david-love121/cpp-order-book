#pragma once
struct Order;
#include <list> 
#include <cstdint>
#include <vector>
struct Trade;
class PriceLevel {
public:
    void AddOrder(Order* order);
    void RemoveOrder(Order* order);
     // This method should handle the logic for filling an order
        // and return a vector of trades that were executed.
    std::vector<Trade> FillOrder(Order* order, uint64_t quantity);   
        
    uint64_t GetTotalVolume() const { return total_volume_; }
    uint64_t GetPrice() const { return price_; }
    Order* GetTopOrder() const {
        if (!order_queue_.empty()) {
            return order_queue_.front();
        }
        return nullptr; // No orders available
    }
private:
    uint64_t price_;
    uint64_t total_volume_;
    std::list<Order*> order_queue_; // Time-priority queue
};

