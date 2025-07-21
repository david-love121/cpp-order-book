#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <cstdint>

class IndicatorLogger {
public:
    IndicatorLogger(const std::string& filename);
    ~IndicatorLogger();

    void WriteHeader(const std::vector<std::string>& headers);
    void WriteRow(uint64_t timestamp, const std::vector<double>& values);

private:
    std::ofstream file_;
};