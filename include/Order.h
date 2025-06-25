#pragma once
#include <cstdint>
#include <list>
class PriceLevel;
struct Order {
    uint64_t order_id;
    uint64_t user_id;
    bool is_buy_side;
    uint64_t quantity;
    uint64_t price;
    uint64_t timestamp;

    // Pointers/iterators for internal management
    //When an order needs to be cancelled, PriceLevel object is accessed through ptr and position from iterator
    PriceLevel* parent_price_level;
    std::list<Order*>::iterator position_in_list;
    //... other fields like user_id, etc.
};