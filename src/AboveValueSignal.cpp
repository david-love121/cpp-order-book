#include "AboveValueSignal.h"

AboveValueSignal::AboveValueSignal(std::shared_ptr<IIndicator> indicator, uint64_t value)
    : ValueSignal(indicator, value) {
    m_name = "AboveValue(" + indicator->name() + ", " + std::to_string(value) + ")";
}

bool AboveValueSignal::is_active() const {
    if (!m_indicator->is_ready()) {
        return false;
    }
    return m_indicator->get_value() > m_value;
}