#include "DatabentoProcessor.h"
#include <iomanip>

DatabentoProcessor::DatabentoProcessor()
    : current_symbol_("ESU4") {
    std::cout << "[DatabentoProcessor] Initialized Databento market data processor" << std::endl;
}

// Core interface implementation
void DatabentoProcessor::SetOrderClient(std::shared_ptr<IClient> order_client) {
    order_client_ = order_client;
}

KeepGoing DatabentoProcessor::ProcessMarketData(const Record &rec) {
    if (!order_client_) {
        return KeepGoing::Continue;
    }
    
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

bool DatabentoProcessor::ClearOrderFromMapping(uint64_t internal_order_id)
{
    uint64_t databento_internal_id = GetDatabentoOrderId(internal_order_id);
    if (databento_internal_id != 0) {
        databento_to_internal_order_id_.erase(databento_internal_id);
        internal_to_databento_order_id_.erase(internal_order_id);
        return true;
    } else {
        return false;
    }

}

KeepGoing DatabentoProcessor::ProcessMboMessage(const MboMsg &mbo)
{
    if (!order_client_) {
        return KeepGoing::Continue;
    }
    
    // Convert symbol using instrument ID
    current_symbol_ = ConvertSymbol(mbo.hd.instrument_id);
    
    // Extract and store timestamp
    uint64_t ts_received = static_cast<uint64_t>(mbo.ts_recv.time_since_epoch().count());
    last_mbo_timestamp_ = ts_received;
    
    try {
        switch (mbo.action) {
            case Action::Add: {
                std::cout << "[MBO-ADD] " << current_symbol_ << " Order " << mbo.order_id 
                          << " " << (mbo.side == Side::Bid ? "BUY" : "SELL") 
                          << " " << mbo.size << "@" << (mbo.price / 1e9) << std::endl;
                
                // Map Databento order ID to internal order ID
                
                
                // Add order directly to OrderBook
                uint64_t internal_order_id = order_client_->SubmitOrder(
                    mbo.order_id,  // databento_order_id
                    databento_user_id,
                    mbo.side == Side::Bid,
                    mbo.size,
                    static_cast<uint64_t>(mbo.price / 1e7), // Convert to ticks
                    ts_received,
                    ts_received
                );
                
                // Check if we should create mapping - only for orders that still exist
                // We can determine this by checking if the internal_order_id is valid
                if (internal_order_id > 0) {
                    // For immediately filled orders, don't create a mapping
                    // This requires OrderBook to return 0 for filled orders or another mechanism
                    MapDatabentoOrderId(mbo.order_id, internal_order_id);
                }
                break;
            }
            case Action::Cancel: {
                std::cout << "[MBO-CANCEL] " << current_symbol_ << " Order " << mbo.order_id << " cancelled" << std::endl;
                
                uint64_t internal_order_id = GetInternalOrderId(mbo.order_id);
                if (internal_order_id != 0) {
                    order_client_->CancelOrder(internal_order_id);
                } else {
                    std::cout << "[MBO-CANCEL-SKIP] Order " << mbo.order_id << " not found" << std::endl;
                }
                break;
            }
            case Action::Modify: {
                std::cout << "[MBO-MODIFY] " << current_symbol_ << " Order " << mbo.order_id 
                          << " modified to " << mbo.size << "@" << (mbo.price / 1e9) << std::endl;
                
                uint64_t internal_order_id = GetInternalOrderId(mbo.order_id);
                if (internal_order_id != 0) {
                    order_client_->ModifyOrder(
                        internal_order_id,
                        mbo.size,
                        static_cast<uint64_t>(mbo.price / 1e7),
                        ts_received,
                        ts_received
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
    // Convert symbol using instrument ID  
    current_symbol_ = ConvertSymbol(trade.hd.instrument_id);
    
    // Extract and store timestamp
    // uint64_t ts_received = static_cast<uint64_t>(trade.ts_recv.time_since_epoch().count());
    
    std::cout << "[TRADE] " << current_symbol_ << " " << trade.size << "@" << (trade.price / 1e9) 
              << " side=" << (trade.side == Side::Bid ? "BUY" : "SELL") << std::endl;
    
    return KeepGoing::Continue;
}

KeepGoing DatabentoProcessor::ProcessQuoteMessage(const Mbp1Msg &mbp1) {
    // Convert symbol using instrument ID
    current_symbol_ = ConvertSymbol(mbp1.hd.instrument_id);
    
    // Extract and store timestamp
    //uint64_t ts_received = static_cast<uint64_t>(mbp1.ts_recv.time_since_epoch().count());
    
    // Access the first level from the BidAskPair array
    const auto &level = mbp1.levels[0];
    
    std::cout << "[QUOTE] " << current_symbol_ 
              << " - Bid: " << (level.bid_px / 1e9) << " (" << level.bid_sz << ")"
              << ", Ask: " << (level.ask_px / 1e9) << " (" << level.ask_sz << ")" << std::endl;
    
    // L3 order book data is processed via MBO messages, quotes are just for informational logging
    
    return KeepGoing::Continue;
}

void DatabentoProcessor::PrintOrderBookStatus() {
    std::cout << "\n=== Market Data Status ===" << std::endl;
    std::cout << "Symbol: " << current_symbol_ << std::endl;
    std::cout << "Last Timestamp: " << last_mbo_timestamp_ << std::endl;
    std::cout << "Active Order Mappings: " << databento_to_internal_order_id_.size() << std::endl;

    std::cout << "=========================" << std::endl;
}

uint64_t DatabentoProcessor::MapDatabentoOrderId(uint64_t databento_order_id, uint64_t internal_order_id) {
    // Check if we already have a mapping
    auto it = databento_to_internal_order_id_.find(databento_order_id);
    if (it != databento_to_internal_order_id_.end()) {
        return it->second;
    }
    
    // Create new mapping

    databento_to_internal_order_id_[databento_order_id] = internal_order_id;
    internal_to_databento_order_id_[internal_order_id] = databento_order_id;
    return internal_order_id;
}
//TODO: Make it so OrderBook manages internal IDs on its own and stores databento IDs with orders
uint64_t DatabentoProcessor::GetInternalOrderId(uint64_t databento_order_id) {
    auto it = databento_to_internal_order_id_.find(databento_order_id);
    return (it != databento_to_internal_order_id_.end()) ? it->second : 0;
}
uint64_t DatabentoProcessor::GetDatabentoOrderId(uint64_t internal_order_id) {
    auto it = internal_to_databento_order_id_.find(internal_order_id);
    return (it != internal_to_databento_order_id_.end()) ? it->second : 0;
}


std::string DatabentoProcessor::ConvertSymbol(uint32_t instrument_id) {
    // For now, return a default symbol
    // In a real implementation, this would use symbol mapping services
    (void)instrument_id; // Suppress unused parameter warning
    return current_symbol_;
}