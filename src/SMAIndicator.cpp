#include "SMAIndicator.h"
#include <numeric>

SMAIndicator::SMAIndicator(const std::string& name) : m_name(name), m_period(20), m_current_value(0) {}

void SMAIndicator::update(uint64_t new_value) {
    m_price_history.push_back(new_value);
    if (m_price_history.size() > static_cast<size_t>(m_period)) {
        m_price_history.erase(m_price_history.begin());
    }

    if (is_ready()) {
        uint64_t sum = std::accumulate(m_price_history.begin(), m_price_history.end(), 0ULL);
        m_current_value = sum / m_price_history.size();
    }
}

void SMAIndicator::update(const OrderBook& /*order_book*/) {
    // Not used for this indicator
}

double SMAIndicator::get_value() const {
    return m_current_value;
}

bool SMAIndicator::is_ready() const {
    return m_price_history.size() == static_cast<size_t>(m_period);
}

void SMAIndicator::configure(const toml::table& config) {
    if (auto period = config["parameters"]["period"].value<int>()) {
        m_period = *period;
    }
}

const std::string& SMAIndicator::name() const {
    return m_name;
}