#include "OrderBookManager.h"
#include "PortfolioManager.h"
#include "TopOfBookTracker.h"
#include "DatabentoProcessor.h"
#include "OrderImbalanceStrategy.h"
#include <iostream>

OrderBookManager::OrderBookManager(uint64_t slippage_delay_ns, uint64_t tracked_user_id)
    : client_id_(1), client_name_("OrderBookManager"), tracked_user_id_(tracked_user_id), slippage_delay_ns_(slippage_delay_ns)
{
    // Initialize owned components
    order_book_ = std::make_shared<OrderBook>();
    portfolio_manager_ = std::make_shared<PortfolioManager>("portfolio_" + std::to_string(tracked_user_id_) + ".csv");
    tob_tracker_ = std::make_shared<TopOfBookTracker>();
    
    
    // Don't register here - will be done in Initialize()
}

OrderBookSnapshot OrderBookManager::GetOrderBookSnapshot() const {
    OrderBookSnapshot snapshot(last_timestamp_, current_symbol_);
    
    if (!order_book_) {
        return snapshot;
    }
    
    // Get bid levels from order book (L3 data)
    auto bid_levels = order_book_->GetBidLevels(20);  // Get up to 20 levels
    for (const auto& [price, level] : bid_levels) {
        snapshot.bid_levels.emplace_back(price, level.GetTotalVolume(), level.GetOrderCount());
    }
    
    // Get ask levels from order book (L3 data)
    auto ask_levels = order_book_->GetAskLevels(20);  // Get up to 20 levels
    for (const auto& [price, level] : ask_levels) {
        snapshot.ask_levels.emplace_back(price, level.GetTotalVolume(), level.GetOrderCount());
    }
    
    return snapshot;
}

// IClient Interface Implementation

uint64_t OrderBookManager::SubmitOrder(uint64_t databento_order_id, uint64_t user_id, bool is_buy, uint64_t quantity, uint64_t price, 
                                uint64_t ts_received, uint64_t ts_executed) {
    if (!order_book_) {
        std::cerr << "[OrderBookManager] Cannot submit order - order book not initialized" << std::endl;
        return 0;
    }
    
    uint64_t internal_order_id = order_book_->AddOrder(databento_order_id, user_id, is_buy, quantity, price, ts_received, ts_executed);
    portfolio_manager_->OnOrderSubmitted(internal_order_id, user_id, is_buy, quantity, price, ts_received);
    std::cout << "[OrderBookManager] Submitting ID " << internal_order_id << " order for user " << user_id 
              << ": " << (is_buy ? "BUY" : "SELL") << " " << quantity 
              << "@" << (price / 100.0) << std::endl;
    return internal_order_id;
}
void OrderBookManager::CancelOrder(uint64_t order_id) {
    if (!order_book_) {
        std::cerr << "[OrderBookManager] Cannot cancel order - order book not initialized" << std::endl;
        return;
    }
    order_book_->CancelOrder(order_id);
    return;
}
void OrderBookManager::ModifyOrder(uint64_t order_id, uint64_t new_quantity, uint64_t new_price, uint64_t new_ts_received, uint64_t new_ts_executed) {
    if (!order_book_) {
        std::cerr << "[OrderBookManager] Cannot modify order - order book not initialized" << std::endl;
        return;
    }
    order_book_->ModifyOrder(order_id, new_quantity, new_price, new_ts_received, new_ts_executed);
    return;
}

uint64_t OrderBookManager::GetBestBid() const {
    return order_book_ ? order_book_->GetBestBid() : 0;
}

uint64_t OrderBookManager::GetBestAsk() const {
    return order_book_ ? order_book_->GetBestAsk() : 0;
}

uint64_t OrderBookManager::GetTotalBidVolume() const {
    return order_book_ ? order_book_->GetTotalBidVolume() : 0;
}

uint64_t OrderBookManager::GetTotalAskVolume() const {
    return order_book_ ? order_book_->GetTotalAskVolume() : 0;
}

uint64_t OrderBookManager::GetSpread() const {
    uint64_t bid = GetBestBid();
    uint64_t ask = GetBestAsk();
    return (bid > 0 && ask > 0) ? (ask - bid) : 0;
}

uint64_t OrderBookManager::GetMidPrice() const {
    uint64_t bid = GetBestBid();
    uint64_t ask = GetBestAsk();
    return (bid > 0 && ask > 0) ? (bid + ask) / 2 : 0;
}

// IClient Event Handlers

void OrderBookManager::OnTradeExecuted(const Trade &trade) {
    // Route to portfolio manager
    RouteToPortfolio(trade);
    if (trade.aggressor_order_closed) {
        data_processor_->ClearOrderFromMapping(trade.aggressor_order_id);
    }
    // Clear the order in DatabentoProcessor mapping
    if (trade.resting_order_closed) {
        data_processor_->ClearOrderFromMapping(trade.resting_order_id);
    }

}

void OrderBookManager::OnOrderAcknowledged(uint64_t /*order_id*/) {
    // Handle order acknowledgment
}

void OrderBookManager::OnOrderCancelled(uint64_t order_id) {
    // Handle order cancellation notification (order already cancelled by OrderBook)
    std::cout << "[OrderBookManager] Order " << order_id << " cancelled - notifying strategy" << std::endl;
    data_processor_->ClearOrderFromMapping(order_id);
    // Notify strategy of order book change
    NotifyStrategyOfOrderBookChange();
}

void OrderBookManager::OnOrderModified(uint64_t order_id, uint64_t new_quantity,
                     uint64_t new_price) {
    // Handle order modification notification (order already modified by OrderBook)
    std::cout << "[OrderBookManager] Order " << order_id << " modified to " 
              << new_quantity << "@" << (new_price / 100.0) << " - notifying strategy" << std::endl;
    
    // Notify strategy of order book change
    NotifyStrategyOfOrderBookChange();
}

void OrderBookManager::OnOrderRejected(uint64_t order_id, const std::string &reason) {
    std::cerr << "Order " << order_id << " rejected: " << reason << std::endl;
}

void OrderBookManager::OnOrderFilled(uint64_t order_id) {
    // Handle order filled notification (order was fully executed and removed)
    std::cout << "[OrderBookManager] Order " << order_id << " filled completely - clearing mapping" << std::endl;
    data_processor_->ClearOrderFromMapping(order_id);
    // Notify strategy of order book change
    NotifyStrategyOfOrderBookChange();
}

void OrderBookManager::OnTopOfBookUpdate(uint64_t best_bid, uint64_t best_ask,
                       uint64_t bid_volume, uint64_t ask_volume) {
    // Route to TOB tracker of order book changes
    RouteToTopOfBookTracker(best_bid, best_ask, bid_volume, ask_volume);

}

void OrderBookManager::Initialize() {
    // Basic initialization without shared_from_this()
    // The shared_ptr registration will be done in InitializeAfterConstruction()
}

void OrderBookManager::InitializeAfterConstruction() {
    // Register ourselves as a client of the order book now that shared_ptr is available
    if (order_book_) {
        try {
            order_book_->RegisterClient(shared_from_this());     
            std::cout << "[OrderBookManager] Successfully registered as order book client" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[OrderBookManager] Error registering as client: " << e.what() << std::endl;
        }
    }
    auto order_imbalance_strategy = std::make_shared<OrderImbalanceStrategy>(
                "Historical_OrderImbalance",
                tracked_user_id_,
                0.10,  // 10% imbalance threshold for real market data
                30     // 30-snapshot lookback window for more data
            );

        // Configure strategy parameters for historical analysis
    order_imbalance_strategy->SetSignalThreshold(0.05);  // 5% signal threshold
    order_imbalance_strategy->SetBaseQuantity(5);        // 5 contracts for trading
    order_imbalance_strategy->SetMomentumFactor(1.2);    // Conservative momentum
    order_imbalance_strategy->SetDecayFactor(0.98);      // Slower decay for analysis
    order_imbalance_strategy->SetSlippageDelay(slippage_delay_ns_); // 2ms slippage to match manager
        // Enable auto-trading with appropriate settings
        
    order_imbalance_strategy->EnableAutoTrading(true);   // Enable auto-trading
    order_imbalance_strategy->SetMinSignalForTrade(0.05); // 5% minimum signal for trading
    order_imbalance_strategy->SetMinOrderInterval(1000000000); // 1 second between orders
    order_imbalance_strategy->SetMaxOrdersPerMinute(10); // Max 10 orders per minute

    order_imbalance_strategy->SetPortfolioManager(portfolio_manager_);
    order_imbalance_strategy->SetOrderClient(shared_from_this());
    SetStrategy(order_imbalance_strategy);
    
}

void OrderBookManager::Shutdown() {
    running_ = false;
    
    // Shutdown components - no specific shutdown needed
}

uint64_t OrderBookManager::GetClientId() const {
    return client_id_;
}

std::string OrderBookManager::GetClientName() const {
    return client_name_;
}

// Orchestrator Management Methods

void OrderBookManager::Start() {
    running_ = true;
    Initialize();
}

void OrderBookManager::Stop() {
    running_ = false;
    Shutdown();
}



// Component access

std::shared_ptr<PortfolioManager> OrderBookManager::GetPortfolioManager() const {
    return portfolio_manager_;
}

std::shared_ptr<TopOfBookTracker> OrderBookManager::GetTopOfBookTracker() const {
    return tob_tracker_;
}

std::shared_ptr<OrderBook> OrderBookManager::GetOrderBook() const {
    return order_book_;
}


// Strategy management

void OrderBookManager::SetStrategy(std::shared_ptr<IStrategy> strategy) {
    strategy_ = strategy;
    if (strategy_ && portfolio_manager_) {
        strategy_->SetPortfolioManager(portfolio_manager_);
    }
}

std::shared_ptr<IStrategy> OrderBookManager::GetStrategy() const {
    return strategy_;
}

// Market data processing

KeepGoing OrderBookManager::ProcessMarketData(const Record& /*record*/) {
    // This would be implemented to process Databento market data
    // For now, return KeepGoing::Continue as placeholder
    return KeepGoing::Continue;
}

// Configuration

uint64_t OrderBookManager::GetTrackedUserId() const {
    return tracked_user_id_;
}

bool OrderBookManager::IsUserTracked(uint64_t user_id) const {
    return user_id == tracked_user_id_;
}

void OrderBookManager::SetSlippageDelay(uint64_t slippage_delay_ns) {
    slippage_delay_ns_ = slippage_delay_ns;
}

uint64_t OrderBookManager::GetSlippageDelay() const {
    return slippage_delay_ns_;
}

// Market state management

void OrderBookManager::UpdateMarketState(const std::string& symbol, uint64_t timestamp) {
    current_symbol_ = symbol;
    last_timestamp_ = timestamp;
}

std::shared_ptr<IClient> OrderBookManager::GetClient() {
    return shared_from_this();
}


// Data processor management

void OrderBookManager::SetDataProcessor(std::shared_ptr<DatabentoProcessor> processor) {
    data_processor_ = processor;
    if (data_processor_ && order_book_) {
        data_processor_->SetOrderClient(shared_from_this());
    }
}

std::shared_ptr<DatabentoProcessor> OrderBookManager::GetDataProcessor() const {
    return data_processor_;
}

// Private methods


void OrderBookManager::RouteToPortfolio(const Trade& trade) {
    if (portfolio_manager_ && IsUserTracked(trade.aggressor_user_id)) {
        portfolio_manager_->OnTradeExecuted(trade);
    }
    if (portfolio_manager_ && IsUserTracked(trade.resting_user_id)) {
        portfolio_manager_->OnTradeExecuted(trade);
    }
}


void OrderBookManager::RouteToTopOfBookTracker(uint64_t best_bid, uint64_t best_ask,
                            uint64_t bid_volume, uint64_t ask_volume) {
    if (tob_tracker_) {
        tob_tracker_->OnTopOfBookUpdate(last_timestamp_, current_symbol_,
                                       best_bid, best_ask, bid_volume, ask_volume);
    }
}

void OrderBookManager::NotifyStrategyOfOrderBookChange() {
    if (strategy_) {
        OrderBookSnapshot l3_snapshot = GetOrderBookSnapshot();
        strategy_->ProcessOrderBookData(l3_snapshot);
    }
}