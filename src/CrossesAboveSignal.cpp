#include "CrossesAboveSignal.h"

CrossesAboveSignal::CrossesAboveSignal(const std::string& name, std::shared_ptr<IIndicator> indicator_a, std::shared_ptr<IIndicator> indicator_b)
    : m_name(name), m_indicator_a(indicator_a), m_indicator_b(indicator_b), m_previous_a(0), m_previous_b(0) {}

bool CrossesAboveSignal::is_active() const {
    if (!m_indicator_a->is_ready() || !m_indicator_b->is_ready()) {
        return false;
    }

    double current_a = m_indicator_a->get_value();
    double current_b = m_indicator_b->get_value();

    bool crossed = (m_previous_a <= m_previous_b) && (current_a > current_b);

    m_previous_a = current_a;
    m_previous_b = current_b;

    return crossed;
}

const std::string& CrossesAboveSignal::name() const {
    return m_name;
}