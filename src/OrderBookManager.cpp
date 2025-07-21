#include "OrderBookManager.h"
#include "PortfolioManager.h"
#include "TopOfBookTracker.h"
#include "DatabentoProcessor.h"
#include "Logger.h"
#include "DatabentoCache.h"
#include "MeanReversionStrategy.h"
#include <utility>

OrderBookManager::OrderBookManager(uint64_t tracked_user_id)
    : client_id_(1), client_name_("OrderBookManager"), tracked_user_id_(tracked_user_id)
{
    // Initialize owned components
    order_book_ = std::make_shared<OrderBook>();
    portfolio_manager_ = std::make_shared<PortfolioManager>(tracked_user_id_, "portfolio_" + std::to_string(tracked_user_id_) + ".csv");
    tob_tracker_ = std::make_shared<TopOfBookTracker>();
    tob_tracker_->EnableCSV("tob_" + std::to_string(tracked_user_id_) + ".csv");
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
    *GLogger << "[OrderBookManager] Order " << order_id << " cancelled - notifying strategy" << '\n';
    data_processor_->ClearOrderFromMapping(order_id);
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
    data_processor_ = std::move(processor);
    if (data_processor_ && order_book_) {
        data_processor_->SetOrderClient(shared_from_this());
    }
}

std::shared_ptr<DatabentoProcessor> OrderBookManager::GetDataProcessor() const {
    return data_processor_;
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
        OrderBookSnapshot l3_snapshot = GetOrderBookSnapshot();
        strategy_->update(l3_snapshot);
    }
}

void OrderBookManager::RunHistoricalDataDemo() {
    *GLogger << "\n=== Historical MBO Data Demo for ES Futures ===" << '\n';
    
    // Check if API key is available
    const char* api_key = std::getenv("DATABENTO_API_KEY");
    bool has_api_key = (api_key && strlen(api_key) > 0);
    
    if (!has_api_key) {
        *GLogger << "No DATABENTO_API_KEY found. Will use cached data only." << '\n';
    } else {
        *GLogger << "API key found. Will use cached data or fetch if needed." << '\n';
    }
    
    try {
        // Initialize cache and parameters
        DatabentoCache cache("databento_cache");
        
        // Historical data parameters
        std::string dataset = "GLBX.MDP3";
        std::string start_time = "2024-06-28T15:30";
        std::string end_time = "2024-06-28T15:35";
        std::vector<std::string> symbols = {"ESU4"};
        Schema schema = Schema::Mbo;
        
        std::string cache_key = cache.generateCacheKey(dataset, start_time, end_time, symbols, schema);
        std::string cache_file_path = cache.getCacheFilePath(cache_key);
        
        *GLogger << "Cache key: " << cache_key << '\n';
        *GLogger << "Cache file: " << cache_file_path << '\n';
        cache.listCache();
        
        if (!data_processor_) {
            data_processor_ = std::make_shared<DatabentoProcessor>();
            SetDataProcessor(data_processor_);
        }
        
        InitializeAfterConstruction();
        
        *GLogger << "Fetching historical MBO data for ES S&P 500 futures..." << '\n';
        
        if (cache.hasCachedData(cache_key)) {
            *GLogger << "\n[CACHE] Loading data from cache file..." << '\n';
            DbnFileStore dbn_store{cache_file_path};
            
            auto record_callback = [this](const Record& record) -> KeepGoing {
                return data_processor_->ProcessMarketData(record);
            };
            
            dbn_store.Replay(record_callback);
        } else {
            if (!has_api_key) {
                *GLogger << "\n[ERROR] No cached data found and no API key available." << '\n';
                return;
            }
            
            *GLogger << "\n[API] Fetching fresh data from Databento API..." << '\n';
            auto client = HistoricalBuilder{}.SetKeyFromEnv().Build();
            auto dbn_store = client.TimeseriesGetRangeToFile(dataset, {start_time, end_time}, symbols, schema, cache_file_path);
            
            auto record_callback = [this](const Record& record) -> KeepGoing {
                return data_processor_->ProcessMarketData(record);
            };
            
            dbn_store.Replay(record_callback);
        }

        portfolio_manager_->PrintPortfolioSummary();
        
    } catch (const std::exception& e) {
        *GLogger << "Historical data demo error: " << e.what() << '\n';
    }
}