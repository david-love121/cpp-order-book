#include "AndRule.h"

AndRule::AndRule(const std::string& name, const std::vector<std::string>& signal_names, trading::Action action, int quantity)
    : m_signal_names(signal_names), m_action(action), m_quantity(quantity) {
    m_name = name;
}

bool AndRule::is_satisfied(const std::map<std::string, std::shared_ptr<ISignal>>& signals, const PortfolioManager& portfolio) const {
    for (const auto& signal_name : m_signal_names) {
        auto it = signals.find(signal_name);
        if (it == signals.end() || !it->second->is_active()) {
            return false;
        }
    }
    return true;
}

trading::Action AndRule::get_action() const {
    return m_action;
}

int AndRule::get_quantity() const {
    return m_quantity;
}