#include "ParquetProcessor.h"
#include <iostream>
#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>
#include <parquet/exception.h>
#include <parquet/file_reader.h>

ParquetProcessor::ParquetProcessor() {
    std::cout << "[ParquetProcessor] Initialized Parquet market data processor" << '\n';
}

void ParquetProcessor::SetOrderClient(std::shared_ptr<IClient> order_client) {
    order_client_ = order_client;
}

void ParquetProcessor::ProcessFile(const std::string& file_path) {
    if (!order_client_) {
        std::cerr << "Order client is not set." << std::endl;
        return;
    }

    try {
        std::shared_ptr<arrow::io::ReadableFile> infile;
        PARQUET_ASSIGN_OR_THROW(infile, arrow::io::ReadableFile::Open(file_path));

        std::unique_ptr<parquet::ParquetFileReader> parquet_reader =
            parquet::ParquetFileReader::OpenFile(file_path);

        std::unique_ptr<parquet::arrow::FileReader> reader;
        PARQUET_THROW_NOT_OK(parquet::arrow::FileReader::Make(
            arrow::default_memory_pool(), std::move(parquet_reader), &reader));

        std::shared_ptr<arrow::Table> table;
        PARQUET_THROW_NOT_OK(reader->ReadTable(&table));

        // Assuming the table has 'Open', 'High', 'Low', 'Close', 'Volume' columns
        auto open_col = table->GetColumnByName("Open");
        auto high_col = table->GetColumnByName("High");
        auto low_col = table->GetColumnByName("Low");
        auto close_col = table->GetColumnByName("Close");
        auto volume_col = table->GetColumnByName("Volume");

        if (!open_col || !high_col || !low_col || !close_col || !volume_col) {
            std::cerr << "One or more required columns are missing from the Parquet file." << std::endl;
            return;
        }

        auto open_array = std::static_pointer_cast<arrow::DoubleArray>(open_col->chunk(0));
        auto close_array = std::static_pointer_cast<arrow::DoubleArray>(close_col->chunk(0));
        
        std::cout << "Processing " << table->num_rows() << " rows from Parquet file." << std::endl;

        for (int64_t i = 0; i < table->num_rows(); ++i) {
            uint64_t price = static_cast<uint64_t>(close_array->Value(i) * 100); // Convert to cents
            uint64_t quantity = 1; // Dummy value
            
            // Create a synthetic order book update
            order_client_->UpdateMarketState("AAPL", i);
            order_client_->SubmitOrder(i, true, quantity, price, i, i);
            order_client_->SubmitOrder(i, false, quantity, price + 1, i, i);
        }

    } catch (const std::exception& e) {
        std::cerr << "Error processing Parquet file: " << e.what() << std::endl;
    }
}