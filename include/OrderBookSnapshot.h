#pragma once

#include <cstdint>
#include <string>
#include <vector>

/**
 * @brief Price level data for L3 order book
 */
struct PriceLevelData {
  uint64_t price;
  uint64_t total_volume;
  uint64_t order_count;
  
  PriceLevelData(uint64_t p = 0, uint64_t vol = 0, uint64_t count = 0)
      : price(p), total_volume(vol), order_count(count) {}
};

/**
 * @brief Full order book snapshot with L3 data
 */
struct OrderBookSnapshot {
  uint64_t timestamp;
  std::string symbol;
  std::vector<PriceLevelData> bid_levels;  // Sorted highest to lowest
  std::vector<PriceLevelData> ask_levels;  // Sorted lowest to highest
  
  OrderBookSnapshot(uint64_t ts = 0, const std::string &sym = "")
      : timestamp(ts), symbol(sym) {}
      
  // Helper methods to extract top-of-book data
  uint64_t GetBestBid() const {
    return bid_levels.empty() ? 0 : bid_levels[0].price;
  }
  
  uint64_t GetBestAsk() const {
    return ask_levels.empty() ? 0 : ask_levels[0].price;
  }
  
  uint64_t GetBestBidVolume() const {
    return bid_levels.empty() ? 0 : bid_levels[0].total_volume;
  }
  
  uint64_t GetBestAskVolume() const {
    return ask_levels.empty() ? 0 : ask_levels[0].total_volume;
  }
  
  uint64_t GetMidPrice() const {
    uint64_t bid = GetBestBid();
    uint64_t ask = GetBestAsk();
    return (bid > 0 && ask > 0) ? (bid + ask) / 2 : 0;
  }
  
  uint64_t GetSpread() const {
    uint64_t bid = GetBestBid();
    uint64_t ask = GetBestAsk();
    return (bid > 0 && ask > 0) ? (ask - bid) : 0;
  }
  
  // Calculate total volume across all levels
  uint64_t GetTotalBidVolume() const {
    uint64_t total = 0;
    for (const auto& level : bid_levels) {
      total += level.total_volume;
    }
    return total;
  }
  
  uint64_t GetTotalAskVolume() const {
    uint64_t total = 0;
    for (const auto& level : ask_levels) {
      total += level.total_volume;
    }
    return total;
  }
};