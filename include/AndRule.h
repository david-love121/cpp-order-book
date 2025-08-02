#pragma once

#include "IRule.h"

class AndRule : public IRule {
public:
    AndRule(const std::string& name, const std::vector<std::string>& signal_names, trading::Action action, int quantity);

    bool is_satisfied(const std::map<std::string, std::shared_ptr<ISignal>>& signals, const PortfolioManager& portfolio) const override;
    trading::Action get_action() const override;
    int get_quantity() const override;

private:
    std::vector<std::string> m_signal_names;
    trading::Action m_action;
    int m_quantity;
};