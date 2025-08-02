#include "RuleLogger.h"

RuleLogger::RuleLogger(std::shared_ptr<IDataSink> data_sink)
    : data_sink_(std::move(data_sink)) {}

void RuleLogger::LogRuleEvaluation(uint64_t timestamp,
                                     const std::string &rule_name,
                                     bool is_satisfied) {
  if (data_sink_) {
    data_sink_->OnData(
        RuleEvaluation(timestamp, rule_name, is_satisfied));
  }
}