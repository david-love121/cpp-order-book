#include "TopOfBookTracker.h"
#include <iostream>
#include <utility>

TopOfBookTracker::TopOfBookTracker(const std::string& symbol, std::shared_ptr<IDataSink> data_sink)
    : symbol_(symbol), data_sink_(std::move(data_sink)) {
}

TopOfBookTracker::~TopOfBookTracker() {
}

void TopOfBookTracker::OnTopOfBookUpdate(uint64_t timestamp, const std::string& symbol, 
                                      uint64_t best_bid, uint64_t best_ask, 
                                      uint64_t bid_volume, uint64_t ask_volume) {
    if (data_sink_) {
        TOBSnapshot snapshot(timestamp, symbol, best_bid / 1e9, best_ask / 1e9, bid_volume, ask_volume);
        data_sink_->OnData(snapshot);
    }
}

void TopOfBookTracker::UpdateSymbol(const std::string& symbol) {
    symbol_ = symbol;
}
