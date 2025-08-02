#include "IStrategy.h"
#include "AndRule.h"
#include "IfRule.h"
#include "Order.h"
#include "SMAIndicator.h"
#include "EMAIndicator.h"
#include "RSIIndicator.h"
#include "BookImbalanceIndicator.h"
#include "CrossesAboveSignal.h"
#include "CrossesBelowSignal.h"
#include "AboveValueSignal.h"
#include "BelowValueSignal.h"
#include "IClient.h"
#include "OrderBookSnapshot.h"
#include "Trade.h"
#include "PortfolioManager.h"
#include "IndicatorLogger.h"
#include "RuleLogger.h"

std::shared_ptr<IRule> parse_rule(const toml::table& rule_config);

Strategy::Strategy(const std::string& name) : m_name(name), m_order_client(nullptr) {}

void Strategy::update(const OrderBook& order_book, uint64_t timestamp) {
    for (auto& [name, indicator] : m_indicators) {
        if (auto book_imbalance_indicator = std::dynamic_pointer_cast<BookImbalanceIndicator>(indicator)) {
            book_imbalance_indicator->update(order_book);
        }
    }
    if (m_indicator_logger) {
        std::vector<double> values;
        for (const auto& [name, indicator] : m_indicators) {
            values.push_back(indicator->get_value());
        }
        m_indicator_logger->WriteRow(timestamp, values);
    }
}

void Strategy::update(const OrderBookSnapshot& order_book_snapshot) {
    uint64_t mid_price = order_book_snapshot.GetMidPrice();
    if (mid_price > 0) {
        for (auto& [name, indicator] : m_indicators) {
            if (dynamic_cast<BookImbalanceIndicator*>(indicator.get()) == nullptr) {
                indicator->update(mid_price);
            }
        }
    }
for (const auto& rule : m_rules) {
    bool is_satisfied = rule->is_satisfied(m_signals, *m_portfolio_manager);
    if (m_rule_logger) {
        m_rule_logger->LogRuleEvaluation(order_book_snapshot.timestamp, rule->get_name(), is_satisfied);
    }
    if (is_satisfied) {
        if (rule->get_action() == trading::Action::BUY) {
            m_order_client->SubmitOrder(m_portfolio_manager->tracked_user_id_, true, rule->get_quantity(), order_book_snapshot.GetBestAsk(), order_book_snapshot.timestamp, order_book_snapshot.timestamp + 1);
        } else if (rule->get_action() == trading::Action::SELL) {
            m_order_client->SubmitOrder(m_portfolio_manager->tracked_user_id_, false, rule->get_quantity(), order_book_snapshot.GetBestBid(), order_book_snapshot.timestamp, order_book_snapshot.timestamp + 1);
        }
    }
    if (m_indicator_logger) {
        std::vector<double> values;
        for (const auto& [name, indicator] : m_indicators) {
            values.push_back(indicator->get_value());
        }
        m_indicator_logger->WriteRow(order_book_snapshot.timestamp, values);
    }
    }
}

void Strategy::update(const Trade& trade) {
    for (auto& [name, indicator] : m_indicators) {
        indicator->update(trade.price);
    }
    if (m_indicator_logger) {
        std::vector<double> values;
        for (const auto& [name, indicator] : m_indicators) {
            values.push_back(indicator->get_value());
        }
        m_indicator_logger->WriteRow(trade.ts_executed, values);
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
            } else if (type == "EMA") {
                auto indicator = std::make_shared<EMAIndicator>();
                indicator->configure(indicator_config);
                m_indicators[name] = indicator;
            } else if (type == "RSI") {
                auto indicator = std::make_shared<RSIIndicator>();
                indicator->configure(indicator_config);
                m_indicators[name] = indicator;
            } else if (type == "BookImbalance") {
                auto indicator = std::make_shared<BookImbalanceIndicator>();
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
                int cooldown = signal_config["cooldown"].value_or(0);
                m_signals[name] = std::make_shared<CrossesAboveSignal>(name, m_indicators[indicator_a_name], m_indicators[indicator_b_name], cooldown);
            } else if (type == "CrossesBelow") {
                std::string indicator_a_name = signal_config["indicator_a"].value_or("");
                std::string indicator_b_name = signal_config["indicator_b"].value_or("");
                int cooldown = signal_config["cooldown"].value_or(0);
                m_signals[name] = std::make_shared<CrossesBelowSignal>(m_indicators[indicator_a_name], m_indicators[indicator_b_name], cooldown);
            } else if (type == "AboveValue") {
                std::string indicator_name = signal_config["indicator"].value_or("");
                uint64_t value = signal_config["value"].value_or<uint64_t>(0);
                m_signals[name] = std::make_shared<AboveValueSignal>(m_indicators[indicator_name], value);
            } else if (type == "BelowValue") {
                std::string indicator_name = signal_config["indicator"].value_or("");
                uint64_t value = signal_config["value"].value_or<uint64_t>(0);
                m_signals[name] = std::make_shared<BelowValueSignal>(m_indicators[indicator_name], value);
            }
        }
    }

    if (auto rules = config["rules"].as_array()) {
        for (auto&& elem : *rules) {
            if (auto rule = parse_rule(*elem.as_table())) {
                m_rules.push_back(rule);
            }
        }
    }
}

toml::table Strategy::to_toml() const {
    return toml::table{};
}

void Strategy::set_order_client(IClient* client) {
    m_order_client = client;
}

void Strategy::set_portfolio_manager(std::shared_ptr<PortfolioManager> portfolio_manager) {
    m_portfolio_manager = portfolio_manager;
    if (m_portfolio_manager) {
        auto data_sink = m_portfolio_manager->GetDataSink();
        m_indicator_logger = std::make_unique<IndicatorLogger>(data_sink);
        m_rule_logger = std::make_unique<RuleLogger>(data_sink);
        std::vector<std::string> headers;
        for (const auto& [name, indicator] : m_indicators) {
            headers.push_back(name);
        }
        m_indicator_logger->WriteHeader(headers);
    }
}

std::shared_ptr<IRule> parse_rule(const toml::table& rule_config) {
    std::string type = rule_config["type"].value_or("");
    std::string name = rule_config["name"].value_or("");

    if (type == "IF") {
        std::string condition_str = rule_config["condition"].value_or("always");
        RuleCondition condition;
        if (condition_str == "if_flat") {
            condition = RuleCondition::IF_FLAT;
        } else if (condition_str == "if_long") {
            condition = RuleCondition::IF_LONG;
        } else if (condition_str == "if_short") {
            condition = RuleCondition::IF_SHORT;
        } else {
            condition = RuleCondition::ALWAYS;
        }
        
        auto sub_rule_config = rule_config["rule"].as_table();
        if (sub_rule_config) {
            auto sub_rule = parse_rule(*sub_rule_config);
            auto rule = std::make_shared<IfRule>(name, condition, sub_rule);
            rule->set_name(name);
            return rule;
        }
    } else if (type == "AND") {
        std::vector<std::string> signal_names;
        if (auto signals = rule_config["signals"].as_array()) {
            for (auto&& signal_elem : *signals) {
                signal_names.push_back(signal_elem.value_or(""));
            }
        }
        std::string action_str = rule_config["action"].value_or("");
        trading::Action action = (action_str == "BUY") ? trading::Action::BUY : trading::Action::SELL;
        int quantity = rule_config["quantity"].value_or(0);
        auto rule = std::make_shared<AndRule>(name, signal_names, action, quantity);
        rule->set_name(name);
        return rule;
    }
    return nullptr;
}
