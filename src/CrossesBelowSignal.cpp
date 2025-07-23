#include "CrossesBelowSignal.h"

CrossesBelowSignal::CrossesBelowSignal(std::shared_ptr<IIndicator> indicator_a, std::shared_ptr<IIndicator> indicator_b)
    : m_indicator_a(indicator_a), m_indicator_b(indicator_b), m_previous_a_value(0), m_previous_b_value(0) {
    m_name = "CrossesBelow(" + indicator_a->name() + ", " + indicator_b->name() + ")";
}

bool CrossesBelowSignal::is_active() const {
    if (!m_indicator_a->is_ready() || !m_indicator_b->is_ready()) {
        return false;
    }

    uint64_t current_a = m_indicator_a->value();
    uint64_t current_b = m_indicator_b->value();

    bool was_above = m_previous_a_value > m_previous_b_value;
    bool is_below = current_a < current_b;

    // Update previous values for the next check
    // This is a bit of a hack, but it's the only way to do it without a non-const update method
    const_cast<CrossesBelowSignal*>(this)->m_previous_a_value = current_a;
    const_cast<CrossesBelowSignal*>(this)->m_previous_b_value = current_b;

    return was_above && is_below;
}

const std::string& CrossesBelowSignal::name() const {
    return m_name;
}