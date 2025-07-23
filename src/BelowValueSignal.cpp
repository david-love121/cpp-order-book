#include "BelowValueSignal.h"

BelowValueSignal::BelowValueSignal(std::shared_ptr<IIndicator> indicator, uint64_t value)
    : ValueSignal(indicator, value) {
    m_name = "BelowValue(" + indicator->name() + ", " + std::to_string(value) + ")";
}

bool BelowValueSignal::is_active() const {
    if (!m_indicator->is_ready()) {
        return false;
    }
    return m_indicator->get_value() < m_value;
}