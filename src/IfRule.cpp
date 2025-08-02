#include "IfRule.h"

IfRule::IfRule(const std::string& name, RuleCondition condition, std::shared_ptr<IRule> rule_to_evaluate)
    : m_condition(condition), m_rule_to_evaluate(rule_to_evaluate) {
    m_name = name;
}

bool IfRule::is_satisfied(const std::map<std::string, std::shared_ptr<ISignal>>& signals, const PortfolioManager& portfolio) const {
    bool condition_met = false;
    int64_t current_position = portfolio.GetRunningPosition();

    switch (m_condition) {
        case RuleCondition::ALWAYS:
            condition_met = true;
            break;
        case RuleCondition::IF_FLAT:
            condition_met = (current_position == 0);
            break;
        case RuleCondition::IF_LONG:
            condition_met = (current_position > 0);
            break;
        case RuleCondition::IF_SHORT:
            condition_met = (current_position < 0);
            break;
    }

    if (condition_met) {
        return m_rule_to_evaluate->is_satisfied(signals, portfolio);
    }

    return false;
}

trading::Action IfRule::get_action() const {
    return m_rule_to_evaluate->get_action();
}

int IfRule::get_quantity() const {
    return m_rule_to_evaluate->get_quantity();
}