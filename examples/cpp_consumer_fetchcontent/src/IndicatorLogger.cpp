#include "IndicatorLogger.h"
#include <iostream>

IndicatorLogger::IndicatorLogger(const std::string& filename) {
    file_.open(filename, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
    }
}

IndicatorLogger::~IndicatorLogger() {
    if (file_.is_open()) {
        file_.close();
    }
}

void IndicatorLogger::WriteHeader(const std::vector<std::string>& headers) {
    if (file_.is_open()) {
        file_ << "timestamp";
        for (const auto& header : headers) {
            file_ << "," << header;
        }
        file_ << std::endl;
    }
}

void IndicatorLogger::WriteRow(uint64_t timestamp, const std::vector<double>& values) {
    if (file_.is_open()) {
        file_ << timestamp;
        for (const auto& value : values) {
            file_ << "," << value;
        }
        file_ << std::endl;
    }
}