#pragma once

#include <string>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <sstream>

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

/**
 * @brief Top of Book tracker for CSV output
 * 
 * Tracks best bid/ask updates and writes them to CSV for market data analysis
 */
class TopOfBookTracker {
private:
    std::string csv_filename_;
    std::ofstream csv_file_;
    bool csv_enabled_;
    std::string symbol_;
    std::string date_range_;
    
    // Helper methods
    void WriteCSVHeader();
    void WriteSnapshotToCSV(const TOBSnapshot& snapshot);
    std::string TimestampToString(uint64_t timestamp_ns) const;
    
public:
    /**
     * @brief Constructor for TOB tracker
     * @param symbol Symbol name for the CSV filename
     * @param date_range Date range string for the CSV filename
     */
    TopOfBookTracker(const std::string& symbol = "", const std::string& date_range = "");
    
    /**
     * @brief Destructor - ensure CSV file is properly closed
     */
    ~TopOfBookTracker();
    
    /**
     * @brief Enable CSV output with custom filename
     * @param filename CSV filename
     */
    void EnableCSV(const std::string& filename);
    
    /**
     * @brief Disable CSV output
     */
    void DisableCSV();
    
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
    
    /**
     * @brief Get CSV filename
     */
    std::string GetCSVFilename() const { return csv_filename_; }
    
    /**
     * @brief Check if CSV logging is enabled
     */
    bool IsCSVEnabled() const { return csv_enabled_; }
};
