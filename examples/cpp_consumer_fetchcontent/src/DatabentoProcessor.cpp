#include "DatabentoProcessor.h"
#include "OrderBookManager.h"
#include <iomanip>

DatabentoProcessor::DatabentoProcessor(OrderBookManager* manager, std::shared_ptr<OrderBook> order_book)
    : manager_(manager), order_book_(order_book) {
    std::cout << "[DatabentoProcessor] Initialized pure processor component" << std::endl;
}

KeepGoing DatabentoProcessor::ProcessMarketData(const Record &rec) {
    // Process record - timestamp will be updated in specific message handlers
    last_mbo_timestamp_ = 0; // Will be set by message processors
    
    switch (rec.RType()) {
        case RType::Mbo: {
            const auto &mbo = rec.Get<MboMsg>();
            return ProcessMboMessage(mbo);
        }
        case RType::Mbp0: {
            const auto &trade = rec.Get<TradeMsg>();
            return ProcessTradeMessage(trade);
        }
        case RType::Mbp1:
        case RType::Bbo1M:
        case RType::Bbo1S: {
            const auto &mbp1 = rec.Get<Mbp1Msg>();
            return ProcessQuoteMessage(mbp1);
        }
        default:
            // Ignore other message types for now
            return KeepGoing::Continue;
    }
}

KeepGoing DatabentoProcessor::ProcessMboMessage(const MboMsg &mbo) {
    if (!order_book_) {
        return KeepGoing::Continue;
    }
    
    // Convert symbol - for now use a simple default
    std::string symbol = "ESU4"; // Default symbol for demo
    
    // Extract and store timestamp
    uint64_t ts_received = static_cast<uint64_t>(mbo.ts_recv.time_since_epoch().count());
    last_mbo_timestamp_ = ts_received;
    
    // Update manager's market state
    if (manager_) {
        manager_->UpdateMarketState(symbol, ts_received);
    }
    
    try {
        switch (mbo.action) {
            case Action::Add: {
                std::cout << "[MBO-ADD] " << symbol << " Order " << mbo.order_id 
                          << " " << (mbo.side == Side::Bid ? "BUY" : "SELL") 
                          << " " << mbo.size << "@" << (mbo.price / 1e9) << std::endl;
                
                // Add order to order book
                uint64_t internal_order_id = MapDatabentoOrderId(mbo.order_id, mbo.order_id);
                order_book_->AddOrder(
                    internal_order_id,
                    0, // System user for market data
                    mbo.side == Side::Bid,
                    mbo.size,
                    static_cast<uint64_t>(mbo.price / 1e7), // Convert to ticks
                    static_cast<uint64_t>(mbo.ts_recv.time_since_epoch().count()),
                    static_cast<uint64_t>(mbo.ts_recv.time_since_epoch().count())
                );
                break;
            }
            case Action::Cancel: {
                std::cout << "[MBO-CANCEL] " << symbol << " Order " << mbo.order_id << " cancelled" << std::endl;
                
                uint64_t internal_order_id = GetInternalOrderId(mbo.order_id);
                if (internal_order_id != 0) {
                    order_book_->CancelOrder(internal_order_id);
                } else {
                    std::cout << "[MBO-CANCEL-SKIP] Order " << mbo.order_id << " not found" << std::endl;
                }
                break;
            }
            case Action::Modify: {
                std::cout << "[MBO-MODIFY] " << symbol << " Order " << mbo.order_id 
                          << " modified to " << mbo.size << "@" << (mbo.price / 1e9) << std::endl;
                
                uint64_t internal_order_id = GetInternalOrderId(mbo.order_id);
                if (internal_order_id != 0) {
                    order_book_->ModifyOrder(
                        internal_order_id,
                        mbo.size,
                        static_cast<uint64_t>(mbo.price / 1e7) // Convert to ticks
                    );
                } else {
                    std::cout << "[MBO-MODIFY-SKIP] Order " << mbo.order_id << " modify failed: Order ID not found" << std::endl;
                }
                break;
            }
            default:
                // Ignore other actions
                break;
        }
    } catch (const std::exception& e) {
        std::cout << "[MBO-ERROR] Failed to process MBO message: " << e.what() << std::endl;
    }
    
    return KeepGoing::Continue;
}

KeepGoing DatabentoProcessor::ProcessTradeMessage(const TradeMsg &trade) {
    std::string symbol = "ESU4"; // Default symbol for demo
    
    // Update manager's market state
    if (manager_) {
        uint64_t ts_received = static_cast<uint64_t>(trade.ts_recv.time_since_epoch().count());
        manager_->UpdateMarketState(symbol, ts_received);
    }
    
    std::cout << "[TRADE] " << symbol << " " << trade.size << "@" << (trade.price / 1e9) 
              << " side=" << (trade.side == Side::Bid ? "BUY" : "SELL") << std::endl;
    
    return KeepGoing::Continue;
}

KeepGoing DatabentoProcessor::ProcessQuoteMessage(const Mbp1Msg &mbp1) {
    std::string symbol = "ESU4"; // Default symbol for demo
    
    // Update manager's market state
    if (manager_) {
        uint64_t ts_received = static_cast<uint64_t>(mbp1.ts_recv.time_since_epoch().count());
        manager_->UpdateMarketState(symbol, ts_received);
    }
    
    // Access the first level from the BidAskPair array
    const auto &level = mbp1.levels[0];
    
    std::cout << "[QUOTE] " << symbol 
              << " - Bid: " << (level.bid_px / 1e9) << " (" << level.bid_sz << ")"
              << ", Ask: " << (level.ask_px / 1e9) << " (" << level.ask_sz << ")" << std::endl;
    
    return KeepGoing::Continue;
}

void DatabentoProcessor::PrintOrderBookStatus() {
    if (!order_book_) return;
    
    std::cout << "\n=== Order Book Status ===" << std::endl;
    
    uint64_t best_bid = order_book_->GetBestBid();
    uint64_t best_ask = order_book_->GetBestAsk();
    
    if (best_bid > 0) {
        std::cout << "Best Bid: " << std::fixed << std::setprecision(2)
                  << (best_bid / 100.0) << std::endl;
    } else {
        std::cout << "Best Bid: No bids" << std::endl;
    }
    
    if (best_ask > 0) {
        std::cout << "Best Ask: " << std::fixed << std::setprecision(2)
                  << (best_ask / 100.0) << std::endl;
    } else {
        std::cout << "Best Ask: No asks" << std::endl;
    }
    
    uint64_t spread = (best_bid > 0 && best_ask > 0) ? (best_ask - best_bid) : 0;
    uint64_t mid = (best_bid > 0 && best_ask > 0) ? (best_bid + best_ask) / 2 : 0;
    std::cout << "Spread: " << std::fixed << std::setprecision(2)
              << (spread / 100.0) << std::endl;
    std::cout << "Mid Price: " << std::fixed << std::setprecision(2)
              << (mid / 100.0) << std::endl;
    std::cout << "Total Bid Volume: " << order_book_->GetTotalBidVolume() << std::endl;
    std::cout << "Total Ask Volume: " << order_book_->GetTotalAskVolume() << std::endl;
    std::cout << "=========================" << std::endl;
}

uint64_t DatabentoProcessor::MapDatabentoOrderId(uint64_t databento_order_id, uint64_t internal_order_id) {
    databento_to_internal_order_id_[databento_order_id] = internal_order_id;
    internal_to_databento_order_id_[internal_order_id] = databento_order_id;
    return internal_order_id;
}

uint64_t DatabentoProcessor::GetInternalOrderId(uint64_t databento_order_id) {
    auto it = databento_to_internal_order_id_.find(databento_order_id);
    return (it != databento_to_internal_order_id_.end()) ? it->second : 0;
}