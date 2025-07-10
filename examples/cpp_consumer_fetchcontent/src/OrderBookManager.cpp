#include "OrderBookManager.h"
#include "DatabentoProcessor.h"
#include <chrono>
#include <iomanip>

OrderBookManager::OrderBookManager(uint64_t slippage_delay_ns, uint64_t tracked_user_id) 
    : client_id_(1), client_name_("OrderBook Manager"), tracked_user_id_(tracked_user_id), 
      slippage_delay_ns_(slippage_delay_ns) {
    
    // Initialize core components
    order_book_ = std::make_shared<OrderBook>();
    
    // Initialize portfolio manager with CSV tracking
    portfolio_manager_ = std::make_shared<PortfolioManager>(
        "portfolio_" + std::to_string(tracked_user_id) + ".csv");
    
    // Initialize TOB tracker with current date
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm *tm = std::gmtime(&time_t);
    
    std::ostringstream date_stream;
    date_stream << std::put_time(tm, "%Y-%m-%d");
    std::string current_date = date_stream.str();
    
    std::string default_symbol = "ES_DEMO";
    tob_tracker_ = std::make_shared<TopOfBookTracker>(
        default_symbol, current_date + "_" + current_date);
    
    // Initialize Databento processor
    databento_processor_ = std::make_shared<DatabentoProcessor>(this, order_book_);
    
    // Note: Client registration will be done in Start() when we have a proper shared_ptr
    
    std::cout << "[OrderBookManager] Initialized orchestrator for user " << tracked_user_id << std::endl;
}

// ========== IClient Interface Implementation ==========

uint64_t OrderBookManager::SubmitOrder(uint64_t user_id, bool is_buy, uint64_t quantity,
                                       uint64_t price, uint64_t ts_received, uint64_t ts_executed) {
    if (!running_ || !order_book_) {
        return 0;
    }
    
    uint64_t order_id = GenerateOrderId();
    try {
        // Notify portfolio manager about the order submission
        if (portfolio_manager_ && IsUserTracked(user_id)) {
            portfolio_manager_->OnOrderSubmitted(order_id, user_id, is_buy, quantity, price, ts_received);
        }
        
        // Submit to order book
        order_book_->AddOrder(order_id, user_id, is_buy, quantity, price, ts_received, ts_executed);
        return order_id;
    } catch (const std::exception& e) {
        std::cout << "[OrderBookManager] Order submission failed: " << e.what() << std::endl;
        return 0;
    }
}

uint64_t OrderBookManager::SubmitOrder(uint64_t user_id, bool is_buy, uint64_t quantity, uint64_t price) {
    auto now = std::chrono::high_resolution_clock::now();
    auto ts = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    return SubmitOrder(user_id, is_buy, quantity, price, ts, ts + slippage_delay_ns_);
}

bool OrderBookManager::CancelOrder(uint64_t order_id) {
    if (!running_ || !order_book_) {
        return false;
    }
    try {
        order_book_->CancelOrder(order_id);
        return true;
    } catch (const std::exception& e) {
        std::cout << "[OrderBookManager] Cancel order failed: " << e.what() << std::endl;
        return false;
    }
}

bool OrderBookManager::ModifyOrder(uint64_t order_id, uint64_t new_quantity, uint64_t new_price) {
    if (!running_ || !order_book_) {
        return false;
    }
    try {
        order_book_->ModifyOrder(order_id, new_quantity, new_price);
        return true;
    } catch (const std::exception& e) {
        std::cout << "[OrderBookManager] Modify order failed: " << e.what() << std::endl;
        return false;
    }
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
    if (!order_book_) return 0;
    uint64_t bid = order_book_->GetBestBid();
    uint64_t ask = order_book_->GetBestAsk();
    return (bid > 0 && ask > 0) ? (ask - bid) : 0;
}

uint64_t OrderBookManager::GetMidPrice() const {
    if (!order_book_) return 0;
    uint64_t bid = order_book_->GetBestBid();
    uint64_t ask = order_book_->GetBestAsk();
    return (bid > 0 && ask > 0) ? (bid + ask) / 2 : 0;
}

// ========== IClient Event Handlers ==========

void OrderBookManager::OnTradeExecuted(const Trade &trade) {
    std::cout << "[OrderBookManager-TRADE]"
              << " aggressor=" << trade.aggressor_user_id << " (Order " << trade.aggressor_order_id << ")"
              << " x resting=" << trade.resting_user_id << " (Order " << trade.resting_order_id << ")"
              << " @ " << std::fixed << std::setprecision(2) << (trade.price / 100.0) 
              << " size=" << trade.quantity << std::endl;
    
    // Route to portfolio manager
    RouteToPortfolio(trade);
}

void OrderBookManager::OnOrderAcknowledged(uint64_t order_id) {
    std::cout << "[OrderBookManager] Order " << order_id << " acknowledged" << std::endl;
}

void OrderBookManager::OnOrderCancelled(uint64_t order_id) {
    std::cout << "[OrderBookManager] Order " << order_id << " cancelled" << std::endl;
}

void OrderBookManager::OnOrderModified(uint64_t order_id, uint64_t new_quantity, uint64_t new_price) {
    std::cout << "[OrderBookManager] Order " << order_id << " modified to " 
              << new_quantity << "@" << (new_price / 100.0) << std::endl;
}

void OrderBookManager::OnOrderRejected(uint64_t order_id, const std::string &reason) {
    std::cout << "[OrderBookManager] Order " << order_id << " rejected: " << reason << std::endl;
}

void OrderBookManager::OnTopOfBookUpdate(uint64_t best_bid, uint64_t best_ask,
                                         uint64_t bid_volume, uint64_t ask_volume) {
    std::cout << "[OrderBookManager-TOB] Bid=" << std::fixed << std::setprecision(2)
              << (best_bid / 100.0) << "(" << bid_volume << ")"
              << ", Ask=" << std::fixed << std::setprecision(2)
              << (best_ask / 100.0) << "(" << ask_volume << ")"
              << ", Mid=" << std::fixed << std::setprecision(2)
              << (GetMidPrice() / 100.0) << ", Spread=" << std::fixed
              << std::setprecision(2) << (GetSpread() / 100.0) << std::endl;
    
    // Route to components
    RouteToTopOfBookTracker(best_bid, best_ask, bid_volume, ask_volume);
    RouteToStrategy(best_bid, best_ask, bid_volume, ask_volume);
}

void OrderBookManager::Initialize() {
    running_ = true;
    std::cout << "[OrderBookManager] " << client_name_ << " initialized (ID: " << client_id_ << ")" << std::endl;
    std::cout << "[OrderBookManager] Tracking user " << tracked_user_id_ << " in portfolio" << std::endl;
}

void OrderBookManager::Shutdown() {
    running_ = false;
    std::cout << "[OrderBookManager] " << client_name_ << " shutting down" << std::endl;
}

uint64_t OrderBookManager::GetClientId() const {
    return client_id_;
}

std::string OrderBookManager::GetClientName() const {
    return client_name_;
}

// ========== Orchestrator Management Methods ==========

void OrderBookManager::Start() {
    Initialize();
}

void OrderBookManager::Stop() {
    Shutdown();
}

std::shared_ptr<PortfolioManager> OrderBookManager::GetPortfolioManager() const {
    return portfolio_manager_;
}

std::shared_ptr<TopOfBookTracker> OrderBookManager::GetTopOfBookTracker() const {
    return tob_tracker_;
}

std::shared_ptr<OrderBook> OrderBookManager::GetOrderBook() const {
    return order_book_;
}

void OrderBookManager::SetStrategy(std::shared_ptr<IStrategy> strategy) {
    strategy_ = strategy;
    std::cout << "[OrderBookManager] Strategy set: " 
              << (strategy ? strategy->GetName() : "None") << std::endl;
}

std::shared_ptr<IStrategy> OrderBookManager::GetStrategy() const {
    return strategy_;
}

KeepGoing OrderBookManager::ProcessMarketData(const Record& record) {
    return databento_processor_->ProcessMarketData(record);
}

std::shared_ptr<IClient> OrderBookManager::GetClient() {
    // Return a wrapper or the current object, depending on usage
    // For now, return nullptr since we're in orchestrator mode
    return nullptr;
}

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

// ========== Private Orchestration Methods ==========

uint64_t OrderBookManager::GenerateOrderId() {
    return next_order_id_.fetch_add(1);
}

void OrderBookManager::RouteToPortfolio(const Trade& trade) {
    if (portfolio_manager_) {
        portfolio_manager_->OnTradeExecuted(trade);
    }
}

void OrderBookManager::RouteToStrategy(uint64_t best_bid, uint64_t best_ask, 
                                      uint64_t bid_volume, uint64_t ask_volume) {
    if (strategy_) {
        // Create TOB snapshot for strategy
        TOBSnapshot tob_snapshot(
            last_timestamp_, // Use the timestamp from the last market data
            current_symbol_.empty() ? "DEMO" : current_symbol_,
            best_bid / 100.0,  // Convert to dollars
            best_ask / 100.0,  // Convert to dollars
            bid_volume,
            ask_volume
        );
        
        std::cout << "[OrderBookManager] Routing TOB to strategy: " 
                  << "Bid=" << (best_bid / 100.0) << ", Ask=" << (best_ask / 100.0) << std::endl;
        
        // Forward to strategy
        strategy_->OnTopOfBookUpdate(tob_snapshot);
    }
}

void OrderBookManager::RouteToTopOfBookTracker(uint64_t best_bid, uint64_t best_ask,
                                              uint64_t bid_volume, uint64_t ask_volume) {
    if (tob_tracker_ && tob_tracker_->IsCSVEnabled()) {
        // Update symbol if we have a current symbol and it's different
        if (!current_symbol_.empty()) {
            tob_tracker_->UpdateSymbol(current_symbol_);
        }
        
        // Use the last timestamp from market data
        uint64_t timestamp = last_timestamp_;
        if (timestamp == 0) {
            timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();
        }
        
        std::string symbol_to_use = current_symbol_.empty() ? "DEMO" : current_symbol_;
        tob_tracker_->OnTopOfBookUpdate(timestamp, symbol_to_use, best_bid, best_ask, bid_volume, ask_volume);
    }
}

void OrderBookManager::UpdateMarketState(const std::string& symbol, uint64_t timestamp) {
    current_symbol_ = symbol;
    last_timestamp_ = timestamp;
}