#pragma once

#include "IDataSink.h"

class InMemorySink : public IDataSink {
public:
    void OnData(const DataRecord& record) override;
const std::vector<PortfolioSnapshot>& GetPortfolioSnapshots() const;
const std::vector<TOBSnapshot>& GetTobSnapshots() const;
const std::vector<Trade>& GetTrades() const;
const std::vector<IndicatorSnapshot>& GetIndicatorSnapshots() const;
const std::vector<RuleEvaluation>& GetRuleEvaluations() const;

private:
std::vector<PortfolioSnapshot> portfolio_snapshots_;
std::vector<TOBSnapshot> tob_snapshots_;
std::vector<Trade> trades_;
std::vector<IndicatorSnapshot> indicator_snapshots_;
std::vector<RuleEvaluation> rule_evaluations_;
};