#include "OrderBookManager.h"
#include "PortfolioManager.h"
#include "TopOfBookTracker.h"
#include "ParquetProcessor.h"
#include "Logger.h"
#include "MeanReversionStrategy.h"
#include <utility>

OrderBookManager::OrderBookManager(uint64_t tracked_user_id, double max_leverage, double initial_cash)
    : client_id_(1), client_name_("OrderBookManager"), tracked_user_id_(tracked_user_id),
      max_leverage_(max_leverage), initial_cash_(initial_cash), slippage_(5000000) //5 ms
{
    // Initialize owned components
    order_book_ = std::make_shared<OrderBook>();
    data_sink_ = std::make_shared<InMemorySink>();
    portfolio_manager_ =
        std::make_shared<PortfolioManager>(tracked_user_id_, data_sink_, max_leverage_, initial_cash_);
    tob_tracker_ = std::make_shared<TopOfBookTracker>("", data_sink_);
    data_processor_ = std::make_shared<ParquetProcessor>();
    //Registration of shared pointers back to this IClient are initialized after construction
}

void OrderBookManager::InitializeAfterConstruction() {
    // Register ourselves as a client of the order book now that shared_ptr is available
    if (order_book_) {
        try {
            order_book_->RegisterClient(shared_from_this());
            *GLogger << "[OrderBookManager] Successfully registered as order book client" << '\n';
        } catch (const std::exception& e) {
            *GLogger << "[OrderBookManager] Error registering as client: " << e.what() << '\n';
        }
    }
    
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

uint64_t OrderBookManager::SubmitOrder(uint64_t user_id, bool is_buy, uint64_t quantity, uint64_t price,
                                uint64_t ts_received, uint64_t ts_executed) {
    if (!order_book_) {
        *GLogger << "[OrderBookManager] Cannot submit order - order book not initialized" << '\n';
        return 0;
    }
    
    uint64_t internal_order_id = order_book_->AddOrder(user_id, is_buy, quantity, price, ts_received, ts_executed);
    portfolio_manager_->OnOrderSubmitted(internal_order_id, user_id, is_buy, quantity, price, ts_received);
    *GLogger << "[OrderBookManager] Submitting ID " << internal_order_id << " order for user " << user_id
              << ": " << (is_buy ? "BUY" : "SELL") << " " << quantity
              << "@" << (price / 100.0) << '\n';
    return internal_order_id;
}
void OrderBookManager::CancelOrder(uint64_t order_id) {
    if (!order_book_) {
        *GLogger << "[OrderBookManager] Cannot cancel order - order book not initialized" << '\n';
        return;
    }
    order_book_->CancelOrder(order_id);
    return;
}
void OrderBookManager::ModifyOrder(uint64_t order_id, uint64_t new_quantity, uint64_t new_price, uint64_t new_ts_received, uint64_t new_ts_executed) {
    if (!order_book_) {
        *GLogger << "[OrderBookManager] Cannot modify order - order book not initialized" << '\n';
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
    *GLogger << "[OrderBookManager] Received trade notification: " << trade.ToString() << '\n';
    // Route to portfolio manager
    RouteToPortfolio(trade);

    if (strategy_) {
        strategy_->update(trade);
    }
}

void OrderBookManager::OnOrderAcknowledged(uint64_t /*order_id*/) {
    // Handle order acknowledgment
}

void OrderBookManager::OnOrderCancelled(uint64_t order_id) {
    // Handle order cancellation notification (order already cancelled by OrderBook)
    *GLogger << "[OrderBookManager] Order " << order_id << " cancelled - notifying strategy" << '\n';
    // Notify strategy of order book change
    NotifyStrategyOfOrderBookChange();
}

void OrderBookManager::OnOrderModified(uint64_t order_id, uint64_t new_quantity,
                     uint64_t new_price) {
    // Handle order modification notification (order already modified by OrderBook)
    *GLogger << "[OrderBookManager] Order " << order_id << " modified to "
              << new_quantity << "@" << (new_price / 100.0) << " - notifying strategy" << '\n';
    
    // Notify strategy of order book change
    NotifyStrategyOfOrderBookChange();
}

void OrderBookManager::OnOrderRejected(uint64_t order_id, const std::string &reason) {
    *GLogger << "Order " << order_id << " rejected: " << reason << '\n';
}

void OrderBookManager::OnOrderFilled(uint64_t order_id) {
    // Handle order filled notification (order was fully executed and removed)
    *GLogger << "[OrderBookManager] Order " << order_id << " filled completely - clearing mapping" << '\n';
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
    strategy_ = std::move(strategy);
    if (strategy_) {
        strategy_->set_portfolio_manager(portfolio_manager_);
        strategy_->set_order_client(this);
    }
}

std::shared_ptr<IStrategy> OrderBookManager::GetStrategy() const {
    return strategy_;
}

// Configuration

uint64_t OrderBookManager::GetTrackedUserId() const {
    return tracked_user_id_;
}

bool OrderBookManager::IsUserTracked(uint64_t user_id) const {
    return user_id == tracked_user_id_;
}



// Market state management

void OrderBookManager::UpdateMarketState(const std::string& symbol, uint64_t timestamp) {
    current_symbol_ = symbol;
    last_timestamp_ = timestamp;
}

std::shared_ptr<IClient> OrderBookManager::GetClient() {
    return shared_from_this();
}

// Private methods


void OrderBookManager::RouteToPortfolio(const Trade& trade) {
    if (portfolio_manager_) {
        portfolio_manager_->OnTrade(trade);
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
        // First, update indicators that require the full order book
        strategy_->update(*order_book_, last_timestamp_);

        // Then, update indicators that work with the snapshot
        OrderBookSnapshot l3_snapshot = GetOrderBookSnapshot();
        strategy_->update(l3_snapshot);
    }
}

void OrderBookManager::RunBacktest(const std::string& data_path) {
    *GLogger << "\n=== Backtest from Parquet File ===" << '\n';
    
    try {
        if (!data_processor_) {
            data_processor_ = std::make_shared<ParquetProcessor>();
        }
        
        data_processor_->SetOrderClient(shared_from_this());
        InitializeAfterConstruction();
        
        *GLogger << "Processing data from: " << data_path << '\n';
        data_processor_->ProcessFile(data_path);

        portfolio_manager_->PrintPortfolioSummary();
        
    } catch (const std::exception& e) {
        *GLogger << "Backtest error: " << e.what() << '\n';
    }
}