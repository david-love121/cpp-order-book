#pragma once

#include "IRule.h"
#include "ISignal.h"

class IfRule : public IRule {
public:
    IfRule(const std::string& name, RuleCondition condition, std::shared_ptr<IRule> rule_to_evaluate);

    bool is_satisfied(const std::map<std::string, std::shared_ptr<ISignal>>& signals, const PortfolioManager& portfolio) const override;
    trading::Action get_action() const override;
    int get_quantity() const override;

private:
    RuleCondition m_condition;
    std::shared_ptr<IRule> m_rule_to_evaluate;
};