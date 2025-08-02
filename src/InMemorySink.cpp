#include "InMemorySink.h"
#include <stdexcept>

void InMemorySink::OnData(const DataRecord& record) {
    std::visit([this](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, PortfolioSnapshot>) {
            portfolio_snapshots_.push_back(arg);
        } else if constexpr (std::is_same_v<T, TOBSnapshot>) {
            tob_snapshots_.push_back(arg);
        } else if constexpr (std::is_same_v<T, Trade>) {
            trades_.push_back(arg);
        } else if constexpr (std::is_same_v<T, IndicatorSnapshot>) {
            indicator_snapshots_.push_back(arg);
        } else if constexpr (std::is_same_v<T, RuleEvaluation>) {
            rule_evaluations_.push_back(arg);
        }
    }, record);
}

const std::vector<PortfolioSnapshot>& InMemorySink::GetPortfolioSnapshots() const {
    return portfolio_snapshots_;
}

const std::vector<TOBSnapshot>& InMemorySink::GetTobSnapshots() const {
    return tob_snapshots_;
}

const std::vector<Trade>& InMemorySink::GetTrades() const {
    return trades_;
}

const std::vector<IndicatorSnapshot>& InMemorySink::GetIndicatorSnapshots() const {
    return indicator_snapshots_;
}

const std::vector<RuleEvaluation>& InMemorySink::GetRuleEvaluations() const {
    return rule_evaluations_;
}