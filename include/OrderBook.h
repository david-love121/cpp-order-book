#pragma once
#include <cstdint>
#include <unordered_map>
#include <map>
#include <list>

// Forward declaration
class PriceLevel;
struct Order;
class OrderBook {
public:
    // Public API for users
    void AddOrder(uint64_t order_id, bool is_buy, uint64_t quantity, uint64_t price);
    void CancelOrder(uint64_t order_id);
    void ModifyOrder(uint64_t order_id, uint64_t new_quantity, uint64_t new_price);

    // Public API for data retrieval
    uint64_t GetBestBid() const;
    uint64_t GetBestAsk() const;
    //... other getters for depth, etc.

private:
    // The core hybrid data structure
    std::unordered_map<uint64_t, Order*> order_map_;
    // Price + PriceLevel obejct
    std::map<uint64_t, PriceLevel, std::greater<uint64_t>> bids_;
    std::map<uint64_t, PriceLevel, std::less<uint64_t>> asks_;
    void AddRestingOrder(Order* order);
    void RemoveRestingOrder(Order* order);
    // Matching logic
    void MatchOrders();
    void MatchWithBids(uint64_t incoming_order_id);
    void MatchWithAsks(uint64_t incoming_order_id);
};
