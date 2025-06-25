#pragma once
struct Order;
#include <list> 
#include <cstdint>
class PriceLevel {
public:
    void AddOrder(Order* order);
    void RemoveOrder(Order* order);
    
private:
    uint64_t price_;
    uint64_t total_volume_;
    std::list<Order*> order_queue_; // Time-priority queue
};

