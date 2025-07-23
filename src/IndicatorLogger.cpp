#include "IndicatorLogger.h"
#include "IndicatorSnapshot.h"
#include <iostream>
#include <utility>

IndicatorLogger::IndicatorLogger(std::shared_ptr<IDataSink> data_sink)
    : data_sink_(std::move(data_sink)) {}

IndicatorLogger::~IndicatorLogger() {}

void IndicatorLogger::WriteHeader(const std::vector<std::string> &headers) {
  headers_ = headers;
}

void IndicatorLogger::WriteRow(uint64_t timestamp,
                               const std::vector<double> &values) {
  if (data_sink_) {
    IndicatorSnapshot snapshot;
    snapshot.timestamp = timestamp;
    snapshot.headers = headers_;
    snapshot.values = values;
    data_sink_->OnData(snapshot);
  }
}