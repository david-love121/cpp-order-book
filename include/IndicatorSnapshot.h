#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct IndicatorSnapshot {
  uint64_t timestamp;
  std::vector<std::string> headers;
  std::vector<double> values;
};