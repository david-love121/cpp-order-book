#include "CrossesBelowSignal.h"

CrossesBelowSignal::CrossesBelowSignal(std::shared_ptr<IIndicator> indicator_a, std::shared_ptr<IIndicator> indicator_b, int cooldown)
    : m_indicator_a(indicator_a), m_indicator_b(indicator_b), m_previous_a_value(0), m_previous_b_value(0), m_cooldown(cooldown), m_cooldown_counter(0) {
    m_name = "CrossesBelow(" + indicator_a->name() + ", " + indicator_b->name() + ")";
}

bool CrossesBelowSignal::is_active() const {
    if (m_cooldown_counter > 0) {
        m_cooldown_counter--;
        return true;
    }

    if (!m_indicator_a->is_ready() || !m_indicator_b->is_ready()) {
        return false;
    }

    double current_a = m_indicator_a->get_value();
    double current_b = m_indicator_b->get_value();

    bool crossed = (m_previous_a_value >= m_previous_b_value) && (current_a < current_b);

    m_previous_a_value = current_a;
    m_previous_b_value = current_b;

    if (crossed) {
        m_cooldown_counter = m_cooldown;
    }

    return crossed;
}

const std::string& CrossesBelowSignal::name() const {
    return m_name;
}