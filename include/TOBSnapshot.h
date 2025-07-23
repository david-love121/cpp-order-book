#pragma once

#include <cstdint>
#include <string>

/**
 * @brief Top of Book snapshot for CSV tracking
 */
struct TOBSnapshot {
    uint64_t timestamp;
    std::string symbol;
    double best_bid;
    double best_ask;
    uint64_t bid_volume;
    uint64_t ask_volume;
    double mid_price;
    double spread;
    
    TOBSnapshot(uint64_t ts = 0, const std::string& sym = "", double bid = 0.0, double ask = 0.0,
                uint64_t bid_vol = 0, uint64_t ask_vol = 0)
        : timestamp(ts), symbol(sym), best_bid(bid), best_ask(ask), 
          bid_volume(bid_vol), ask_volume(ask_vol),
          mid_price((bid > 0 && ask > 0) ? (bid + ask) / 2.0 : 0.0),
          spread((bid > 0 && ask > 0) ? (ask - bid) : 0.0) {}
};