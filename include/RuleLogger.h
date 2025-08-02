#pragma once

#include "IDataSink.h"
#include "RuleEvaluation.h"
#include <memory>
#include <string>
#include <vector>

class RuleLogger {
public:
  explicit RuleLogger(std::shared_ptr<IDataSink> data_sink);

  void LogRuleEvaluation(uint64_t timestamp, const std::string &rule_name,
                         bool is_satisfied);

private:
  std::shared_ptr<IDataSink> data_sink_;
};