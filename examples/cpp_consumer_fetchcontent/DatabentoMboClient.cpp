#include "DatabentoMboClient.h"
#include <ctime>
#include <iomanip>

DatabentoMboClient::DatabentoMboClient(uint64_t client_id,
                                       const std::string &name,
                                       std::shared_ptr<OrderBook> order_book,
                                       uint64_t tracked_user_id,
                                       uint64_t slippage_delay_ns)
    : order_book_(order_book), client_id_(client_id), client_name_(name),
      tracked_user_id_(tracked_user_id) {
  // Note: slippage_delay_ns parameter is reserved for future use
  (void)slippage_delay_ns; // Suppress unused parameter warning

  portfolio_manager_ = std::make_shared<PortfolioManager>(
      "portfolio_" + std::to_string(tracked_user_id) + ".csv");

  // Generate current date for TOB CSV filename
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  std::tm *tm = std::gmtime(&time_t);

  std::ostringstream date_stream;
  date_stream << std::put_time(tm, "%Y-%m-%d");
  std::string current_date = date_stream.str();

  // Initialize TOB tracker with default symbol and current date
  // Symbol will be updated when we process market data
  std::string default_symbol = "ES_DEMO";
  tob_tracker_ = std::make_shared<TopOfBookTracker>(
      default_symbol, current_date + "_" + current_date);

  std::cout
      << "[DatabentoMboClient] Initialized with order ID tracking for user "
      << tracked_user_id << std::endl;
}

// ========== IClient Interface Implementation ==========

uint64_t DatabentoMboClient::SubmitOrder(uint64_t user_id, bool is_buy,
                                         uint64_t quantity, uint64_t price,
                                         uint64_t ts_received,
                                         uint64_t ts_executed) {
  if (!running_ || !order_book_) {
    return 0;
  }

  uint64_t order_id = GenerateOrderId();
  try {
    // Notify portfolio manager about the order submission
    if (portfolio_manager_) {
      portfolio_manager_->OnOrderSubmitted(order_id, user_id, is_buy, quantity,
                                           price, ts_received);
    }

    order_book_->AddOrder(order_id, user_id, is_buy, quantity, price,
                          ts_received, ts_executed);
    return order_id;
  } catch (const std::exception &e) {
    return 0;
  }
}

uint64_t DatabentoMboClient::SubmitOrder(uint64_t user_id, bool is_buy,
                                         uint64_t quantity, uint64_t price) {
  if (!running_ || !order_book_) {
    return 0;
  }

  uint64_t order_id = GenerateOrderId();
  try {
    // Notify portfolio manager about the order submission
    if (portfolio_manager_) {
      portfolio_manager_->OnOrderSubmitted(order_id, user_id, is_buy, quantity,
                                           price, 0);
    }

    order_book_->AddOrder(order_id, user_id, is_buy, quantity, price);
    return order_id;
  } catch (const std::exception &e) {
    return 0;
  }
}

bool DatabentoMboClient::CancelOrder(uint64_t order_id) {
  if (!running_ || !order_book_) {
    return false;
  }

  try {
    order_book_->CancelOrder(order_id);
    return true;
  } catch (const std::exception &e) {
    return false;
  }
}

bool DatabentoMboClient::ModifyOrder(uint64_t order_id, uint64_t new_quantity,
                                     uint64_t new_price) {
  if (!running_ || !order_book_) {
    return false;
  }

  try {
    order_book_->ModifyOrder(order_id, new_quantity, new_price);
    return true;
  } catch (const std::exception &e) {
    return false;
  }
}

uint64_t DatabentoMboClient::GetBestBid() const {
  if (!order_book_)
    return 0;
  return order_book_->GetBestBid();
}

uint64_t DatabentoMboClient::GetBestAsk() const {
  if (!order_book_)
    return 0;
  return order_book_->GetBestAsk();
}

uint64_t DatabentoMboClient::GetTotalBidVolume() const {
  if (!order_book_)
    return 0;
  return order_book_->GetTotalBidVolume();
}

uint64_t DatabentoMboClient::GetTotalAskVolume() const {
  if (!order_book_)
    return 0;
  return order_book_->GetTotalAskVolume();
}

uint64_t DatabentoMboClient::GetSpread() const {
  uint64_t bid = GetBestBid();
  uint64_t ask = GetBestAsk();
  if (bid == 0 || ask == 0)
    return 0;
  return ask - bid;
}

uint64_t DatabentoMboClient::GetMidPrice() const {
  uint64_t bid = GetBestBid();
  uint64_t ask = GetBestAsk();
  if (bid == 0 || ask == 0)
    return 0;
  return (bid + ask) / 2;
}

// ========== IClient Event Handlers ==========

void DatabentoMboClient::OnTradeExecuted(const Trade &trade) {
  std::cout << "[CLIENT-TRADE] Aggressor=" << trade.aggressor_user_id
            << " (Order " << trade.aggressor_order_id
            << ") x Resting=" << trade.resting_user_id << " (Order "
            << trade.resting_order_id << ")"
            << " @ " << std::fixed << std::setprecision(2)
            << (trade.price / 100.0) << " size=" << trade.quantity << std::endl;

  // Notify portfolio manager about the trade (it will only track if orders are
  // being monitored)
  if (portfolio_manager_) {
    portfolio_manager_->OnTradeExecuted(trade);
  }
}

void DatabentoMboClient::OnOrderAcknowledged(uint64_t order_id) {
  (void)order_id; // Suppress unused parameter warning
}

void DatabentoMboClient::OnOrderCancelled(uint64_t order_id) {
  (void)order_id; // Suppress unused parameter warning
}

void DatabentoMboClient::OnOrderModified(uint64_t order_id,
                                         uint64_t new_quantity,
                                         uint64_t new_price) {
  (void)order_id;
  (void)new_quantity;
  (void)new_price; // Suppress unused parameter warnings
}

void DatabentoMboClient::OnOrderRejected(uint64_t order_id,
                                         const std::string &reason) {
  if (reason.find("already exists") == std::string::npos &&
      reason.find("not found") == std::string::npos) {
    std::cout << "[CLIENT-REJECT] Order " << order_id << " rejected: " << reason
              << std::endl;
  }
}

void DatabentoMboClient::OnTopOfBookUpdate(uint64_t best_bid, uint64_t best_ask,
                                           uint64_t bid_volume,
                                           uint64_t ask_volume) {
  std::cout << "[CLIENT-TOB] Bid=" << std::fixed << std::setprecision(2)
            << (best_bid / 100.0) << "(" << bid_volume << ")"
            << ", Ask=" << std::fixed << std::setprecision(2)
            << (best_ask / 100.0) << "(" << ask_volume << ")"
            << ", Mid=" << std::fixed << std::setprecision(2)
            << (GetMidPrice() / 100.0) << ", Spread=" << std::fixed
            << std::setprecision(2) << (GetSpread() / 100.0) << std::endl;

  // Track TOB updates for CSV export if tracker is enabled
  if (tob_tracker_ && tob_tracker_->IsCSVEnabled()) {
    // Update symbol if we have a current symbol and it's different
    if (!current_symbol_.empty()) {
      tob_tracker_->UpdateSymbol(current_symbol_);
    }

    // Use the last MBO timestamp instead of current system time
    uint64_t timestamp = last_mbo_timestamp_;
    if (timestamp == 0) {
      timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();
    }

    std::string symbol_to_use =
        current_symbol_.empty() ? "DEMO" : current_symbol_;
    tob_tracker_->OnTopOfBookUpdate(timestamp, symbol_to_use, best_bid,
                                    best_ask, bid_volume, ask_volume);
  }
}

void DatabentoMboClient::Initialize() {
  running_ = true;
  std::cout << "[CLIENT] " << client_name_ << " initialized (ID: " << client_id_
            << ")" << std::endl;

  // Print which user is being tracked
  if (portfolio_manager_) {
    std::cout << "[CLIENT] Tracking user " << tracked_user_id_
              << " in portfolio" << std::endl;
  }

  PrintOrderBookStatus();
}

void DatabentoMboClient::Shutdown() {
  running_ = false;
  std::cout << "[CLIENT] " << client_name_ << " shutting down" << std::endl;
  PrintOrderBookStatus();

  // Print portfolio summary on shutdown
  if (portfolio_manager_) {
    portfolio_manager_->PrintPortfolioSummary();
  }
}

uint64_t DatabentoMboClient::GetClientId() const { return client_id_; }

std::string DatabentoMboClient::GetClientName() const { return client_name_; }

// Portfolio management access
std::shared_ptr<PortfolioManager>
DatabentoMboClient::GetPortfolioManager() const {
  return portfolio_manager_;
}

std::shared_ptr<TopOfBookTracker>
DatabentoMboClient::GetTopOfBookTracker() const {
  return tob_tracker_;
}

uint64_t DatabentoMboClient::GetTrackedUserId() const {
  return tracked_user_id_;
}

bool DatabentoMboClient::IsUserTracked(uint64_t user_id) const {
  if (portfolio_manager_) {
    return (user_id == PortfolioManager::TRACKED_USER_ID);
  }
  return false;
}

// ========== Databento MBO Message Processing ==========

KeepGoing DatabentoMboClient::ProcessMarketData(const Record &rec) {
  symbol_mappings_.OnRecord(rec);

  if (!running_) {
    return KeepGoing::Stop;
  }

  // Handle different record types based on RType
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
    // Ignore other message types
    break;
  }

  return KeepGoing::Continue;
}

uint64_t DatabentoMboClient::GenerateOrderId() {
  return next_order_id_.fetch_add(1);
}

KeepGoing DatabentoMboClient::ProcessMboMessage(const MboMsg &mbo) {
  std::string symbol = symbol_mappings_[mbo.hd.instrument_id];

  // Debug: Print symbol mapping info
  static int debug_count = 0;
  if (debug_count < 10) {
    std::cout << "[DEBUG] Instrument ID: " << mbo.hd.instrument_id
              << " -> Symbol: '" << symbol << "'" << std::endl;
    debug_count++;
  }

  // If symbol mapping fails, try to use a default symbol for ES futures
  if (symbol.empty()) {
    // For demonstration purposes, assume this is ES futures data
    symbol = "ESU4"; // Use the expected symbol
  }

  // Update current symbol for portfolio tracking
  current_symbol_ = symbol;

  // Extract timestamps that are common to all MBO actions
  uint64_t ts_received =
      static_cast<uint64_t>(mbo.ts_recv.time_since_epoch().count());
  uint64_t ts_executed =
      ts_received + static_cast<uint64_t>(mbo.ts_in_delta.count());

  // Store the latest timestamp for use in TopOfBookUpdate callbacks
  last_mbo_timestamp_ = ts_executed;

  // Handle different MBO actions
  switch (mbo.action) {
  case Action::Add: {
    int64_t price_raw = static_cast<int64_t>(mbo.price);
    // For ES futures: Databento prices are in nano-precision, convert to index
    // points ES futures trade in 0.25 point increments (e.g., 5432.25, 5432.50,
    // etc.)
    double price_points = static_cast<double>(price_raw) / 1000000000.0;
    int64_t price_for_orderbook = static_cast<int64_t>(
        price_points * 100); // Store as hundredths for precision
    uint64_t size = static_cast<uint64_t>(mbo.size);
    uint64_t order_id = static_cast<uint64_t>(mbo.order_id);

    // Debug: Print timestamp info for first few orders to verify conversion
    static int ts_debug_count = 0;
    if (ts_debug_count < 5) {
      std::cout << "[TIMESTAMP-DEBUG] Order " << order_id
                << " - ts_received: " << ts_received
                << " ns, ts_executed: " << ts_executed
                << " ns, delta: " << mbo.ts_in_delta.count() << " ns"
                << std::endl;
      ts_debug_count++;
    }

    bool is_buy = (mbo.side == Side::Bid);

    try {
      order_book_->AddOrder(order_id, 1, is_buy, size, price_for_orderbook,
                            ts_received, ts_executed);
      std::cout << "[MBO-ADD] " << symbol << " Order " << order_id << " "
                << (is_buy ? "BUY" : "SELL") << " " << size << "@" << std::fixed
                << std::setprecision(2) << price_points << std::endl;
    } catch (const std::exception &e) {
      // Order might already exist, which is fine for market data
      std::cout << "[MBO-ADD-SKIP] Order " << order_id << " already exists"
                << std::endl;
    }
    break;
  }

  case Action::Cancel: {
    uint64_t order_id = static_cast<uint64_t>(mbo.order_id);
    try {
      order_book_->CancelOrder(order_id);
      std::cout << "[MBO-CANCEL] " << symbol << " Order " << order_id
                << " cancelled" << std::endl;
    } catch (const std::exception &e) {
      // Order might not exist, which is fine for market data
      std::cout << "[MBO-CANCEL-SKIP] Order " << order_id << " not found"
                << std::endl;
    }
    break;
  }

  case Action::Modify: {
    uint64_t order_id = static_cast<uint64_t>(mbo.order_id);
    // Fix price conversion for ES futures
    double new_price_points = static_cast<double>(mbo.price) / 1000000000.0;
    int64_t new_price_for_orderbook =
        static_cast<int64_t>(new_price_points * 100);
    uint64_t new_size = static_cast<uint64_t>(mbo.size);

    try {
      order_book_->ModifyOrder(order_id, new_size, new_price_for_orderbook);
      std::cout << "[MBO-MODIFY] " << symbol << " Order " << order_id
                << " modified to " << new_size << "@" << std::fixed
                << std::setprecision(2) << new_price_points << std::endl;
    } catch (const std::exception &e) {
      std::cout << "[MBO-MODIFY-SKIP] Order " << order_id
                << " modify failed: " << e.what() << std::endl;
    }
    break;
  }

  default:
    break;
  }

  // Print periodic status updates
  static int mbo_count = 0;
  if (++mbo_count % 100 == 0) {
    printOrderBookStatus();
  }

  return KeepGoing::Continue;
}

KeepGoing DatabentoMboClient::ProcessTradeMessage(const TradeMsg &trade) {
  std::string symbol = symbol_mappings_[trade.hd.instrument_id];

  if (symbol.empty()) {
    return KeepGoing::Continue;
  }

  // Extract timestamp for trade message
  uint64_t ts_received =
      static_cast<uint64_t>(trade.ts_recv.time_since_epoch().count());
  uint64_t ts_executed = ts_received; // Trade messages don't have ts_in_delta

  // Store the latest timestamp for use in TopOfBookUpdate callbacks
  last_mbo_timestamp_ = ts_executed;

  uint64_t price = static_cast<uint64_t>(trade.price / 1000000000);
  uint64_t size = static_cast<uint64_t>(trade.size);

  std::cout << "\n[TRADE] " << symbol << " - Price: " << (price / 100.0)
            << ", Size: " << size << std::endl;

  // Update last price for this symbol
  last_price_by_symbol_[symbol] = price;

  printOrderBookStatus();

  return KeepGoing::Continue;
}

KeepGoing DatabentoMboClient::ProcessQuoteMessage(const Mbp1Msg &mbp1) {
  std::string symbol = symbol_mappings_[mbp1.hd.instrument_id];

  if (symbol.empty()) {
    return KeepGoing::Continue;
  }

  // Extract timestamp for quote message
  uint64_t ts_received =
      static_cast<uint64_t>(mbp1.ts_recv.time_since_epoch().count());
  uint64_t ts_executed = ts_received; // Quote messages don't have ts_in_delta

  // Store the latest timestamp for use in TopOfBookUpdate callbacks
  last_mbo_timestamp_ = ts_executed;

  // Access the first level from the BidAskPair array
  const auto &level = mbp1.levels[0];

  std::cout << "\n[MARKET DATA] Quote for " << symbol
            << " - Bid: " << (level.bid_px / 1e9) << " (" << level.bid_sz << ")"
            << ", Ask: " << (level.ask_px / 1e9) << " (" << level.ask_sz << ")"
            << std::endl;

  printOrderBookStatus();

  return KeepGoing::Continue;
}

void DatabentoMboClient::PrintOrderBookStatus() {
  if (!order_book_)
    return;

  std::cout << "\n=== Order Book Status ===" << std::endl;

  uint64_t best_bid = GetBestBid();
  uint64_t best_ask = GetBestAsk();

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

  if (best_bid > 0 && best_ask > 0) {
    std::cout << "Spread: " << std::fixed << std::setprecision(2)
              << (GetSpread() / 100.0) << std::endl;
    std::cout << "Mid Price: " << std::fixed << std::setprecision(2)
              << (GetMidPrice() / 100.0) << std::endl;
  }

  std::cout << "Total Bid Volume: " << GetTotalBidVolume() << std::endl;
  std::cout << "Total Ask Volume: " << GetTotalAskVolume() << std::endl;
  std::cout << "=========================" << std::endl;
}

// Alias for compatibility with original code style
void DatabentoMboClient::printOrderBookStatus() { PrintOrderBookStatus(); }
