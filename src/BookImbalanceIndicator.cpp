#include "BookImbalanceIndicator.h"
#include <stdexcept>

BookImbalanceIndicator::BookImbalanceIndicator() : m_depth(0), m_imbalance(0.0) {}

void BookImbalanceIndicator::update(const OrderBook& order_book) {
    if (m_depth == 0) {
        return; // Not configured
    }

    auto bid_levels = order_book.GetBidLevels(m_depth);
    auto ask_levels = order_book.GetAskLevels(m_depth);

    uint64_t total_bid_volume = 0;
    for (const auto& level : bid_levels) {
        total_bid_volume += level.second.GetTotalVolume();
    }

    uint64_t total_ask_volume = 0;
    for (const auto& level : ask_levels) {
        total_ask_volume += level.second.GetTotalVolume();
    }

    if (total_bid_volume + total_ask_volume == 0) {
        m_imbalance = 0.5; // Neutral imbalance
    } else {
        m_imbalance = static_cast<double>(total_bid_volume) / (total_bid_volume + total_ask_volume);
    }
}

// This indicator uses the full order book, not a single value.
void BookImbalanceIndicator::update(uint64_t /*new_value*/) {
    // No-op
}

double BookImbalanceIndicator::get_value() const {
    return m_imbalance;
}

bool BookImbalanceIndicator::is_ready() const {
    // This indicator is always ready as long as it's configured.
    return m_depth > 0;
}

void BookImbalanceIndicator::configure(const toml::table& config) {
    m_name = config["name"].value_or<std::string>("");
    if (auto params = config["parameters"].as_table()) {
        m_depth = params->get("depth")->value_or<int>(0);
    }
}

const std::string& BookImbalanceIndicator::name() const {
    return m_name;
}