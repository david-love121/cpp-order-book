#include "ValueSignal.h"

ValueSignal::ValueSignal(std::shared_ptr<IIndicator> indicator, uint64_t value)
    : m_indicator(indicator), m_value(value) {}

const std::string& ValueSignal::name() const {
    return m_name;
}