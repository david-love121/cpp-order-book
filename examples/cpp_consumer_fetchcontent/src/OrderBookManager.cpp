#include "OrderBookManager.h"

// OrderBookManagerClient implementation
void OrderBookManagerClient::OnTopOfBookCallback(uint64_t best_bid, uint64_t best_ask,
                                                 uint64_t bid_volume, uint64_t ask_volume) {
    if (manager_) {
        manager_->OnTopOfBookUpdate(best_bid, best_ask, bid_volume, ask_volume);
    }
}

// OrderBookManager implementation
OrderBookManager::OrderBookManager(uint64_t slippage_delay_ns, uint64_t tracked_user_id) {
    order_book_ = std::make_shared<OrderBook>();
    client_ = std::make_shared<OrderBookManagerClient>(1, "OrderBook Manager", order_book_, tracked_user_id, slippage_delay_ns, this);
    // Register the client with the order book for callbacks
    order_book_->RegisterClient(client_);
}

void OrderBookManager::Start() {
    // Initialize the client
    client_->Initialize();
}

void OrderBookManager::Stop() {
    // Shutdown the client
    client_->Shutdown();
}

// Bridge method for Databento callbacks
KeepGoing OrderBookManager::OnMarketData(const Record& record) {
    return client_->ProcessMarketData(record);
}

// Provide access to client for additional operations
std::shared_ptr<IClient> OrderBookManager::GetClient() {
    return client_;
}

// Strategy management
void OrderBookManager::SetStrategy(std::shared_ptr<IStrategy> strategy) {
    strategy_ = strategy;
}

std::shared_ptr<IStrategy> OrderBookManager::GetStrategy() const {
    return strategy_;
}

// TOB callback handler (called by OrderBookManagerClient)
void OrderBookManager::OnTopOfBookUpdate(uint64_t best_bid, uint64_t best_ask,
                                         uint64_t bid_volume, uint64_t ask_volume) {
    if (strategy_) {
        // Create TOB snapshot for strategy
        TOBSnapshot tob_snapshot(
            client_->GetPublicLastMboTimestamp(), // Use the timestamp from the last MBO message
            client_->GetPublicCurrentSymbol().empty() ? "DEMO" : client_->GetPublicCurrentSymbol(),
            best_bid / 100.0,  // Convert to dollars
            best_ask / 100.0,  // Convert to dollars
            bid_volume,
            ask_volume
        );
        
        std::cout << "[OrderBookManager] Forwarding TOB update to strategy: " 
                  << "Bid=" << (best_bid / 100.0) << ", Ask=" << (best_ask / 100.0) << std::endl;
        
        // Forward to strategy
        strategy_->OnTopOfBookUpdate(tob_snapshot);
    } else {
        std::cout << "[OrderBookManager] No strategy set, skipping TOB update" << std::endl;
    }
}