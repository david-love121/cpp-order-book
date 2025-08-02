#pragma once

#include <string>
#include <memory>
#include "IClient.h"

#include <arrow/table.h>

class ParquetProcessor {
public:
    ParquetProcessor();
    void SetOrderClient(std::shared_ptr<IClient> order_client);
    void ProcessFile(const std::string& file_path);

private:
    void ProcessOHLCVData(const std::shared_ptr<arrow::Table>& table);
    void ProcessMBP10Data(const std::shared_ptr<arrow::Table>& table);
    void ProcessMBP1Data(const std::shared_ptr<arrow::Table>& table);
    std::shared_ptr<IClient> order_client_;
};