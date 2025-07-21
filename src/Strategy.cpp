#include "IStrategy.h"
#include "SMAIndicator.h"
#include "CrossesAboveSignal.h"
#include "IClient.h"
#include "OrderBookSnapshot.h"
#include "Trade.h"
#include "PortfolioManager.h"

Strategy::Strategy(const std::string& name) : m_name(name), m_order_client(nullptr) {}

void Strategy::update(const OrderBookSnapshot& order_book) {
    uint64_t mid_price = order_book.GetMidPrice();
    if (mid_price > 0) {
        for (auto& [name, indicator] : m_indicators) {
            indicator->update(mid_price);
        }
    }

    for (const auto& rule : m_rules) {
        if (m_signals[rule.signal_name]->is_active()) {
            if (rule.action == trading::Action::BUY) {
                m_order_client->SubmitOrder(m_portfolio_manager->tracked_user_id_, true, rule.quantity, order_book.GetBestAsk(), 0, 0);
            } else if (rule.action == trading::Action::SELL) {
                m_order_client->SubmitOrder(m_portfolio_manager->tracked_user_id_, false, rule.quantity, order_book.GetBestBid(), 0, 0);
            }
        }
    }
}

void Strategy::update(const Trade& trade) {
    for (auto& [name, indicator] : m_indicators) {
        indicator->update(trade.price);
    }
}

void Strategy::from_toml(const toml::table& config) {
    if (auto indicators = config["indicators"].as_array()) {
        for (auto&& elem : *indicators) {
            const auto& indicator_config = *elem.as_table();
            std::string type = indicator_config["type"].value_or("");
            std::string name = indicator_config["name"].value_or("");

            if (type == "SMA") {
                auto indicator = std::make_shared<SMAIndicator>(name);
                indicator->configure(indicator_config);
                m_indicators[name] = indicator;
            }
        }
    }

    if (auto signals = config["signals"].as_array()) {
        for (auto&& elem : *signals) {
            const auto& signal_config = *elem.as_table();
            std::string type = signal_config["type"].value_or("");
            std::string name = signal_config["name"].value_or("");

            if (type == "CrossesAbove") {
                std::string indicator_a_name = signal_config["indicator_a"].value_or("");
                std::string indicator_b_name = signal_config["indicator_b"].value_or("");
                m_signals[name] = std::make_shared<CrossesAboveSignal>(name, m_indicators[indicator_a_name], m_indicators[indicator_b_name]);
            }
        }
    }

    if (auto rules = config["rules"].as_array()) {
        for (auto&& elem : *rules) {
            const auto& rule_config = *elem.as_table();
            std::string signal_name = rule_config["signal"].value_or("");
            std::string action_str = rule_config["action"].value_or("");
            int quantity = rule_config["quantity"].value_or(0);

            trading::Action action = (action_str == "BUY") ? trading::Action::BUY : trading::Action::SELL;
            m_rules.push_back({signal_name, action, quantity});
        }
    }
}

toml::table Strategy::to_toml() const {
    // This is a simplified implementation for now
    return toml::table{};
}

void Strategy::set_order_client(IClient* client) {
    m_order_client = client;
}

void Strategy::set_portfolio_manager(std::shared_ptr<PortfolioManager> portfolio_manager) {
    m_portfolio_manager = portfolio_manager;
}


