#pragma once

#include <string>
#include <memory>

#include "IDataSink.h"
#include "TOBSnapshot.h"

/**
 * @brief Top of Book tracker
 * 
 * Tracks best bid/ask updates and passes them to a data sink
 */
class TopOfBookTracker {
private:
    std::string symbol_;
    std::shared_ptr<IDataSink> data_sink_;
    
public:
    /**
     * @brief Constructor for TOB tracker
     * @param symbol Symbol name for the tracker
     * @param data_sink Shared pointer to a data sink
     */
    TopOfBookTracker(const std::string& symbol, std::shared_ptr<IDataSink> data_sink);
    
    /**
     * @brief Destructor
     */
    ~TopOfBookTracker();
    
    /**
     * @brief Record a top of book update
     * @param timestamp Timestamp of the update
     * @param symbol Symbol name
     * @param best_bid Best bid price in ticks
     * @param best_ask Best ask price in ticks
     * @param bid_volume Volume at best bid
     * @param ask_volume Volume at best ask
     */
    void OnTopOfBookUpdate(uint64_t timestamp, const std::string& symbol, 
                          uint64_t best_bid, uint64_t best_ask, 
                          uint64_t bid_volume, uint64_t ask_volume);
    
    /**
     * @brief Update the symbol for better CSV filename generation
     * @param symbol New symbol name
     */
    void UpdateSymbol(const std::string& symbol);
};
