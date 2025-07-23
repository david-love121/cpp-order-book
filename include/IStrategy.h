#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "Trade.h"
#include "OrderBook.h"
#include <toml++/toml.h>
#include "IIndicator.h"
#include <map>
#include "ISignal.h"

// Forward declarations
class PortfolioManager;
class IClient;
struct Trade;
struct OrderBookSnapshot;

class IStrategy {
public:
    virtual ~IStrategy() = default;

    virtual void update(const OrderBook& order_book) = 0;
    virtual void update(const OrderBookSnapshot& order_book_snapshot) = 0;
    virtual void update(const Trade& trade) = 0;

    virtual void from_toml(const toml::table& config) = 0;
    virtual toml::table to_toml() const = 0;

    virtual void set_order_client(IClient* client) = 0;
    virtual void set_portfolio_manager(std::shared_ptr<PortfolioManager> portfolio_manager) = 0;
};

class Strategy : public IStrategy {
public:
    Strategy(const std::string& name);

    void update(const OrderBook& order_book) override;
    void update(const OrderBookSnapshot& order_book_snapshot) override;
    void update(const Trade& trade) override;

    void from_toml(const toml::table& config) override;
    toml::table to_toml() const override;

    void set_order_client(IClient* client) override;
    void set_portfolio_manager(std::shared_ptr<PortfolioManager> portfolio_manager) override;

protected:
    std::string m_name;
    IClient* m_order_client;
    std::shared_ptr<PortfolioManager> m_portfolio_manager;
    std::map<std::string, std::shared_ptr<IIndicator>> m_indicators;
    std::map<std::string, std::shared_ptr<ISignal>> m_signals;
    std::vector<Rule> m_rules;
};
