#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations
struct TOBSnapshot;
class PortfolioManager;

/**
 * @brief Market data snapshot for strategy calculations
 */
struct MarketSnapshot {
  uint64_t timestamp;
  std::string symbol;
  double best_bid;
  double best_ask;
  uint64_t bid_volume;
  uint64_t ask_volume;
  double mid_price;
  double spread;
  double
      order_imbalance; // (bid_volume - ask_volume) / (bid_volume + ask_volume)

  MarketSnapshot(uint64_t ts = 0, const std::string &sym = "", double bid = 0.0,
                 double ask = 0.0, uint64_t bid_vol = 0, uint64_t ask_vol = 0)
      : timestamp(ts), symbol(sym), best_bid(bid), best_ask(ask),
        bid_volume(bid_vol), ask_volume(ask_vol),
        mid_price((bid > 0 && ask > 0) ? (bid + ask) / 2.0 : 0.0),
        spread((bid > 0 && ask > 0) ? (ask - bid) : 0.0),
        order_imbalance((bid_vol + ask_vol > 0)
                            ? static_cast<double>(bid_vol - ask_vol) /
                                  (bid_vol + ask_vol)
                            : 0.0) {}
};

/**
 * @brief Strategy signal enumeration
 */
enum class StrategySignal {
  NONE, // No action
  BUY,  // Buy signal
  SELL, // Sell signal
  HOLD  // Hold position
};

/**
 * @brief Strategy signal with quantity
 */
struct StrategyAction {
  StrategySignal signal;
  uint64_t quantity;
  double confidence; // Signal confidence [0.0, 1.0]

  StrategyAction(StrategySignal sig = StrategySignal::NONE, uint64_t qty = 0,
                 double conf = 0.0)
      : signal(sig), quantity(qty), confidence(conf) {}
};

/**
 * @brief Mathematical function type for user-defined strategies
 * Takes market snapshot and returns a signal value [-1.0, 1.0]
 * -1.0 = strong sell, 0.0 = neutral, 1.0 = strong buy
 */
using MathFunction = std::function<double(const MarketSnapshot &)>;

/**
 * @brief Abstract base class for trading strategies
 */
class Strategy {
protected:
  std::string name_;
  uint64_t user_id_;
  bool enabled_;
  double signal_threshold_; // Minimum signal strength to act
  uint64_t base_quantity_;  // Base quantity for orders
  std::shared_ptr<PortfolioManager> portfolio_manager_;

  // Mathematical function for signal generation
  MathFunction math_function_;

  // Strategy parameters (can be customized per strategy)
  std::unordered_map<std::string, double> parameters_;

public:
  /**
   * @brief Constructor for Strategy
   * @param name Strategy name
   * @param user_id User ID for this strategy
   * @param math_func Mathematical function for signal generation
   * @param portfolio_mgr Portfolio manager for tracking
   */
  Strategy(const std::string &name, uint64_t user_id, MathFunction math_func,
           std::shared_ptr<PortfolioManager> portfolio_mgr = nullptr);

  virtual ~Strategy() = default;

  /**
   * @brief Process market data and generate trading signals
   * @param snapshot Market data snapshot
   * @return Strategy action
   */
  virtual StrategyAction ProcessMarketData(const MarketSnapshot &snapshot);

  /**
   * @brief Update strategy with TOB data
   * @param tob_snapshot Top of book snapshot
   * @return Strategy action
   */
  StrategyAction OnTopOfBookUpdate(const TOBSnapshot &tob_snapshot);

  /**
   * @brief Set/update mathematical function
   * @param func New mathematical function
   */
  void SetMathFunction(MathFunction func) { math_function_ = func; }

  /**
   * @brief Set strategy parameter
   * @param key Parameter name
   * @param value Parameter value
   */
  void SetParameter(const std::string &key, double value) {
    parameters_[key] = value;
  }

  /**
   * @brief Get strategy parameter
   * @param key Parameter name
   * @param default_value Default value if parameter not found
   * @return Parameter value
   */
  double GetParameter(const std::string &key, double default_value = 0.0) const;

  /**
   * @brief Enable/disable strategy
   * @param enabled Strategy enabled state
   */
  void SetEnabled(bool enabled) { enabled_ = enabled; }

  /**
   * @brief Set signal threshold
   * @param threshold Minimum signal strength to act [0.0, 1.0]
   */
  void SetSignalThreshold(double threshold) { signal_threshold_ = threshold; }

  /**
   * @brief Set base quantity for orders
   * @param quantity Base quantity
   */
  void SetBaseQuantity(uint64_t quantity) { base_quantity_ = quantity; }

  /**
   * @brief Set portfolio manager
   * @param portfolio_mgr Portfolio manager
   */
  void SetPortfolioManager(std::shared_ptr<PortfolioManager> portfolio_mgr) {
    portfolio_manager_ = portfolio_mgr;
  }

  // Getters
  const std::string &GetName() const { return name_; }
  uint64_t GetUserId() const { return user_id_; }
  bool IsEnabled() const { return enabled_; }
  double GetSignalThreshold() const { return signal_threshold_; }
  uint64_t GetBaseQuantity() const { return base_quantity_; }
  std::shared_ptr<PortfolioManager> GetPortfolioManager() const {
    return portfolio_manager_;
  }

protected:
  /**
   * @brief Convert signal value to strategy action
   * @param signal_value Signal value [-1.0, 1.0]
   * @return Strategy action
   */
  virtual StrategyAction SignalToAction(double signal_value);
};

/**
 * @brief Order imbalance strategy implementation
 */
class OrderImbalanceStrategy : public Strategy {
private:
  double lookback_period_; // Lookback period for moving average
  std::vector<double> imbalance_history_;

public:
  /**
   * @brief Constructor for OrderImbalanceStrategy
   * @param user_id User ID for this strategy
   * @param portfolio_mgr Portfolio manager for tracking
   * @param lookback_period Lookback period for moving average
   */
  OrderImbalanceStrategy(
      uint64_t user_id,
      std::shared_ptr<PortfolioManager> portfolio_mgr = nullptr,
      double lookback_period = 10.0);

  /**
   * @brief Create the mathematical function for order imbalance
   * @return Order imbalance mathematical function
   */
  static MathFunction CreateOrderImbalanceFunction(double threshold = 0.1);
};

/**
 * @brief Mean reversion strategy implementation
 */
class MeanReversionStrategy : public Strategy {
private:
  std::vector<double> price_history_;
  double lookback_period_;

public:
  /**
   * @brief Constructor for MeanReversionStrategy
   * @param user_id User ID for this strategy
   * @param portfolio_mgr Portfolio manager for tracking
   * @param lookback_period Lookback period for mean calculation
   */
  MeanReversionStrategy(
      uint64_t user_id,
      std::shared_ptr<PortfolioManager> portfolio_mgr = nullptr,
      double lookback_period = 20.0);

  /**
   * @brief Create the mathematical function for mean reversion
   * @return Mean reversion mathematical function
   */
  static MathFunction
  CreateMeanReversionFunction(double std_dev_threshold = 2.0);
};

/**
 * @brief Strategy manager for handling multiple strategies per user
 */
class StrategyManager {
private:
  std::unordered_map<uint64_t, std::shared_ptr<Strategy>> user_strategies_;
  std::vector<std::shared_ptr<Strategy>> all_strategies_;

public:
  /**
   * @brief Add strategy for a user
   * @param user_id User ID
   * @param strategy Strategy to add
   */
  void AddStrategy(uint64_t user_id, std::shared_ptr<Strategy> strategy);

  /**
   * @brief Remove strategy for a user
   * @param user_id User ID
   */
  void RemoveStrategy(uint64_t user_id);

  /**
   * @brief Get strategy for a user
   * @param user_id User ID
   * @return Strategy pointer or nullptr if not found
   */
  std::shared_ptr<Strategy> GetStrategy(uint64_t user_id) const;

  /**
   * @brief Process market data for all strategies
   * @param snapshot Market data snapshot
   * @return Vector of strategy actions from all active strategies
   */
  std::vector<std::pair<uint64_t, StrategyAction>>
  ProcessMarketData(const MarketSnapshot &snapshot);

  /**
   * @brief Get all strategies
   * @return Vector of all strategies
   */
  const std::vector<std::shared_ptr<Strategy>> &GetAllStrategies() const {
    return all_strategies_;
  }

  /**
   * @brief Clear all strategies
   */
  void Clear();
};