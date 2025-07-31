#pragma once

#include <string>
#include <memory>
#include "IClient.h"

class ParquetProcessor {
public:
    ParquetProcessor();
    void SetOrderClient(std::shared_ptr<IClient> order_client);
    void ProcessFile(const std::string& file_path);

private:
    std::shared_ptr<IClient> order_client_;
};