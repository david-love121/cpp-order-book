#pragma once

#include "IDataSink.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class IndicatorLogger {
public:
  IndicatorLogger(std::shared_ptr<IDataSink> data_sink);
  ~IndicatorLogger();

  void WriteHeader(const std::vector<std::string> &headers);
  void WriteRow(uint64_t timestamp, const std::vector<double> &values);

private:
  std::shared_ptr<IDataSink> data_sink_;
  std::vector<std::string> headers_;
};