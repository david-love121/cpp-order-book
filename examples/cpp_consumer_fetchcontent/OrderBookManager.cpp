#include "OrderBookManager.h"

OrderBookManager::OrderBookManager(uint64_t slippage_delay_ns) {
    order_book_ = std::make_shared<OrderBook>();
    // Use the user ID defaults to 0: the ID for non tracked users
    uint64_t tracked_user_id = 0;
    client_ = std::make_shared<DatabentoMboClient>(1, "Databento MBO Client", order_book_, tracked_user_id, slippage_delay_ns);
    // Register the client with the order book for callbacks
    order_book_->RegisterClient(client_);
}

void OrderBookManager::Start() {
    // Client is already initialized via RegisterClient
}

void OrderBookManager::Stop() {
    // Client will be shutdown via UnregisterClient in OrderBook destructor
    order_book_->UnregisterClient(client_->GetClientId());
}

// Bridge method for Databento callbacks
KeepGoing OrderBookManager::OnMarketData(const Record& record) {
    return client_->ProcessMarketData(record);
}

// Provide access to client for additional operations
std::shared_ptr<IClient> OrderBookManager::GetClient() {
    return client_;
}
