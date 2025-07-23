#include "EMAIndicator.h"
#include <numeric>
#include <stdexcept>

EMAIndicator::EMAIndicator() : m_period(0), m_smoothing_factor(0.0), m_current_value(0) {}

void EMAIndicator::update(uint64_t new_value) {
    if (m_period == 0) {
        return; // Not configured yet
    }

    if (m_values.size() < m_period) {
        m_values.push_back(new_value);
        if (m_values.size() == m_period) {
            // Calculate initial SMA
            uint64_t sum = std::accumulate(m_values.begin(), m_values.end(), 0ULL);
            m_current_value = sum / m_period;
        }
    } else {
        m_current_value = (new_value * m_smoothing_factor) + (m_current_value * (1 - m_smoothing_factor));
        m_values.erase(m_values.begin());
        m_values.push_back(new_value);
    }
}

void EMAIndicator::update(const OrderBook& /*order_book*/) {
    // Not used for this indicator
}

double EMAIndicator::get_value() const {
    return m_current_value;
}

bool EMAIndicator::is_ready() const {
    return m_values.size() == m_period;
}

void EMAIndicator::configure(const toml::table& config) {
    m_name = config["name"].value_or<std::string>("");
    if (auto params = config["parameters"].as_table()) {
        m_period = params->get("period")->value_or<int>(0);
        if (m_period > 0) {
            m_smoothing_factor = 2.0 / (m_period + 1);
        }
    }
}

const std::string& EMAIndicator::name() const {
    return m_name;
}