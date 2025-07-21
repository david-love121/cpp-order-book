#pragma once
#include <cstdint>
#include <unordered_map>
#include <map>
#include <list>
#include <atomic>
#include <vector>
#include <memory>

#include "PriceLevel.h"
// Forward declaration
struct Order;
struct Trade;
class IClient;
class OrderBook {
public:
    // Constructor and destructor
    OrderBook() : next_order_id_(1000) {}
    ~OrderBook(); // Destructor to clean up remaining orders
    
    // Disable copy/move to avoid issues with raw pointers
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) = delete;
    OrderBook& operator=(OrderBook&&) = delete;
    
    // Public API for users
    uint64_t AddOrder(uint64_t user_id, bool is_buy, uint64_t quantity, uint64_t price,
                      uint64_t ts_received, uint64_t ts_executed);
    void CancelOrder(uint64_t order_id);
    
    void ModifyOrder(uint64_t order_id, uint64_t new_quantity, uint64_t new_price, uint64_t new_ts_received, uint64_t new_ts_executed);

    // Client management
    void RegisterClient(std::shared_ptr<IClient> client);
    void UnregisterClient(uint64_t client_id);

    // Public API for data retrieval
    uint64_t GetBestBid() const;
    uint64_t GetBestAsk() const;
    //... other getters for depth, etc.

    uint64_t GetTotalBidVolume() const;
    uint64_t GetTotalAskVolume() const;
    
    // L3 Order Book data access methods
    std::vector<std::pair<uint64_t, PriceLevel>> GetBidLevels(size_t max_levels = 20) const;
    std::vector<std::pair<uint64_t, PriceLevel>> GetAskLevels(size_t max_levels = 20) const;

private:
    // The core hybrid data structure
    std::unordered_map<uint64_t, Order*> order_map_; // Internal ID -> Order
    // Price + PriceLevel obejct
    std::map<uint64_t, PriceLevel, std::greater<uint64_t>> bids_;
    std::map<uint64_t, PriceLevel, std::less<uint64_t>> asks_;
    
    // Client management
    std::unordered_map<uint64_t, std::shared_ptr<IClient>> clients_;
    
    // Order ID generation
    std::atomic<uint64_t> next_order_id_;
    
    void AddRestingOrder(Order* order);
    void RemoveRestingOrder(Order* order);
    // Matching logic
    std::vector<Trade> MatchOrders(Order* incoming_order);
    
    // Client notification methods
    void NotifyTradeExecuted(const Trade& trade);
    void NotifyOrderAcknowledged(uint64_t order_id);
    void NotifyOrderCancelled(uint64_t order_id);
    void NotifyOrderModified(uint64_t order_id, uint64_t new_quantity, uint64_t new_price);
    void NotifyOrderRejected(uint64_t order_id, const std::string& reason);
    void NotifyOrderFilled(uint64_t order_id);
    void NotifyTopOfBookUpdate();
   

};
