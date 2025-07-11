#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations
struct TOBSnapshot;
class PortfolioManager;

/**
 * @brief Price level data for L3 order book
 */
struct PriceLevelData {
  uint64_t price;
  uint64_t total_volume;
  uint64_t order_count;
  
  PriceLevelData(uint64_t p = 0, uint64_t vol = 0, uint64_t count = 0)
      : price(p), total_volume(vol), order_count(count) {}
};

/**
 * @brief Full order book snapshot with L3 data
 */
struct OrderBookSnapshot {
  uint64_t timestamp;
  std::string symbol;
  std::vector<PriceLevelData> bid_levels;  // Sorted highest to lowest
  std::vector<PriceLevelData> ask_levels;  // Sorted lowest to highest
  
  OrderBookSnapshot(uint64_t ts = 0, const std::string &sym = "")
      : timestamp(ts), symbol(sym) {}
      
  // Helper methods to extract top-of-book data
  double GetBestBid() const {
    return bid_levels.empty() ? 0.0 : bid_levels[0].price / 100.0;
  }
  
  double GetBestAsk() const {
    return ask_levels.empty() ? 0.0 : ask_levels[0].price / 100.0;
  }
  
  uint64_t GetBestBidVolume() const {
    return bid_levels.empty() ? 0 : bid_levels[0].total_volume;
  }
  
  uint64_t GetBestAskVolume() const {
    return ask_levels.empty() ? 0 : ask_levels[0].total_volume;
  }
  
  double GetMidPrice() const {
    double bid = GetBestBid();
    double ask = GetBestAsk();
    return (bid > 0 && ask > 0) ? (bid + ask) / 2.0 : 0.0;
  }
  
  double GetSpread() const {
    double bid = GetBestBid();
    double ask = GetBestAsk();
    return (bid > 0 && ask > 0) ? (ask - bid) : 0.0;
  }
  
  // Calculate total volume across all levels
  uint64_t GetTotalBidVolume() const {
    uint64_t total = 0;
    for (const auto& level : bid_levels) {
      total += level.total_volume;
    }
    return total;
  }
  
  uint64_t GetTotalAskVolume() const {
    uint64_t total = 0;
    for (const auto& level : ask_levels) {
      total += level.total_volume;
    }
    return total;
  }
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
 * @brief Abstract base class interface for trading strategies
 * 
 * This interface defines the contract that all trading strategies must implement.
 * Concrete strategy implementations should inherit from this class and provide
 * their own logic for processing market data and generating trading signals.
 */
class IStrategy {
public:
  virtual ~IStrategy() = default;

  /**
   * @brief Process full order book data and generate trading signals
   * @param orderbook_snapshot Full order book snapshot with L3 data
   * @return Strategy action
   */
  virtual StrategyAction ProcessOrderBookData(const OrderBookSnapshot &orderbook_snapshot) = 0;

  /**
   * @brief Initialize strategy with parameters
   * @param parameters Strategy-specific parameters
   */
  virtual void Initialize(const std::unordered_map<std::string, double> &parameters) = 0;

  /**
   * @brief Reset strategy state
   */
  virtual void Reset() = 0;

  /**
   * @brief Enable/disable strategy
   * @param enabled Strategy enabled state
   */
  virtual void SetEnabled(bool enabled) = 0;

  /**
   * @brief Set portfolio manager
   * @param portfolio_mgr Portfolio manager
   */
  virtual void SetPortfolioManager(std::shared_ptr<PortfolioManager> portfolio_mgr) = 0;

  // Getters
  virtual const std::string &GetName() const = 0;
  virtual uint64_t GetUserId() const = 0;
  virtual bool IsEnabled() const = 0;
  virtual std::shared_ptr<PortfolioManager> GetPortfolioManager() const = 0;
};

/**
 * @brief Base strategy implementation providing common functionality
 * 
 * This class provides a concrete implementation of common strategy functionality
 * while still requiring derived classes to implement the core ProcessMarketData method.
 */
class Strategy : public IStrategy {
protected:
  std::string name_;
  uint64_t user_id_;
  bool enabled_;
  double signal_threshold_;
  uint64_t base_quantity_;
  std::shared_ptr<PortfolioManager> portfolio_manager_;
  std::unordered_map<std::string, double> parameters_;


public:
  /**
   * @brief Constructor for Strategy
   * @param name Strategy name
   * @param user_id User ID for this strategy
   */
  Strategy(const std::string &name, uint64_t user_id);

  virtual ~Strategy() = default;

  // Pure virtual - must be implemented by derived classes
  virtual StrategyAction ProcessOrderBookData(const OrderBookSnapshot &orderbook_snapshot) = 0;

  // Interface implementations
  virtual void Initialize(const std::unordered_map<std::string, double> &parameters) override;
  virtual void Reset() override;
  virtual void SetEnabled(bool enabled) override;
  virtual void SetPortfolioManager(std::shared_ptr<PortfolioManager> portfolio_mgr) override;

  // Getters
  virtual const std::string &GetName() const override;
  virtual uint64_t GetUserId() const override;
  virtual bool IsEnabled() const override;
  virtual std::shared_ptr<PortfolioManager> GetPortfolioManager() const override;

  // Additional helper methods for derived classes
  void SetParameter(const std::string &key, double value);
  double GetParameter(const std::string &key, double default_value = 0.0) const;
  void SetSignalThreshold(double threshold);
  double GetSignalThreshold() const;
  void SetBaseQuantity(uint64_t quantity);
  uint64_t GetBaseQuantity() const;

protected:
  /**
   * @brief Convert signal value to strategy action (helper for derived classes)
   * @param signal_value Signal value [-1.0, 1.0]
   * @return Strategy action
   */
  virtual StrategyAction SignalToAction(double signal_value);
};

/**
 * @brief Strategy manager for handling multiple strategies per user
 */
class StrategyManager {
private:
  std::unordered_map<uint64_t, std::shared_ptr<IStrategy>> user_strategies_;
  std::vector<std::shared_ptr<IStrategy>> all_strategies_;

public:
  /**
   * @brief Add strategy for a user
   * @param user_id User ID
   * @param strategy Strategy to add
   */
  void AddStrategy(uint64_t user_id, std::shared_ptr<IStrategy> strategy);

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
  std::shared_ptr<IStrategy> GetStrategy(uint64_t user_id) const;

  /**
   * @brief Process order book data for all strategies
   * @param orderbook_snapshot Order book snapshot with L3 data
   * @return Vector of strategy actions from all active strategies
   */
  std::vector<std::pair<uint64_t, StrategyAction>>
  ProcessOrderBookData(const OrderBookSnapshot &orderbook_snapshot);

  /**
   * @brief Get all strategies
   * @return Vector of all strategies
   */
  const std::vector<std::shared_ptr<IStrategy>> &GetAllStrategies() const {
    return all_strategies_;
  }

  /**
   * @brief Clear all strategies
   */
  void Clear();
};