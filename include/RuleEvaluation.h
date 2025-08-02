#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct RuleEvaluation {
  uint64_t timestamp;
  std::string rule_name;
  bool is_satisfied;

  RuleEvaluation(uint64_t ts, const std::string &name, bool satisfied)
      : timestamp(ts), rule_name(name), is_satisfied(satisfied) {}
};