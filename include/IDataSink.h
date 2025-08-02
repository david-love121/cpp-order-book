#pragma once

#include "RuleEvaluation.h"
#include "IndicatorSnapshot.h"
#include "PortfolioSnapshot.h"
#include "TOBSnapshot.h"
#include "Trade.h"
#include <variant>
#include <vector>

// Define data types that can be handled by the sink
using DataRecord =
    std::variant<PortfolioSnapshot, TOBSnapshot, Trade, IndicatorSnapshot, RuleEvaluation>;

class IDataSink {
public:
    virtual ~IDataSink() = default;
    virtual void OnData(const DataRecord& record) = 0;
};