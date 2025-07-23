#include "RSIIndicator.h"
#include <stdexcept>

RSIIndicator::RSIIndicator() : m_period(0), m_previous_value(0), m_avg_gain(0.0), m_avg_loss(0.0) {}

void RSIIndicator::update(uint64_t new_value) {
    if (m_period == 0) {
        return; // Not configured
    }

    if (m_previous_value == 0) {
        m_previous_value = new_value;
        return;
    }

    double change = static_cast<double>(new_value) - m_previous_value;
    double gain = (change > 0) ? change : 0.0;
    double loss = (change < 0) ? -change : 0.0;

    m_previous_value = new_value;

    if (m_values.size() < m_period) {
        m_values.push_back(new_value);
        m_avg_gain = (m_avg_gain * (m_values.size() - 1) + gain) / m_values.size();
        m_avg_loss = (m_avg_loss * (m_values.size() - 1) + loss) / m_values.size();
    } else {
        m_avg_gain = (m_avg_gain * (m_period - 1) + gain) / m_period;
        m_avg_loss = (m_avg_loss * (m_period - 1) + loss) / m_period;
    }
}

uint64_t RSIIndicator::value() const {
    if (m_avg_loss == 0) {
        return 100;
    }
    double rs = m_avg_gain / m_avg_loss;
    return static_cast<uint64_t>(100.0 - (100.0 / (1.0 + rs)));
}

bool RSIIndicator::is_ready() const {
    return m_values.size() >= m_period;
}

void RSIIndicator::configure(const toml::table& config) {
    m_name = config["name"].value_or<std::string>("");
    if (auto params = config["parameters"].as_table()) {
        m_period = params->get("period")->value_or<int>(0);
    }
}

const std::string& RSIIndicator::name() const {
    return m_name;
}