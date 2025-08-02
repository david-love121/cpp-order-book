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

        auto data_type_col = table->GetColumnByName("data_type");
        if (!data_type_col) {
            std::cerr << "data_type column is missing from the Parquet file." << std::endl;
            return;
        }

        auto data_type_array = std::static_pointer_cast<arrow::StringArray>(data_type_col->chunk(0));
        std::string data_type = data_type_array->GetString(0);

        if (data_type == "ohlcv") {
            ProcessOHLCVData(table);
        } else if (data_type == "mbp-10") {
            ProcessMBP10Data(table);
        } else if (data_type == "mbp-1") {
            ProcessMBP1Data(table);
        } else {
            std::cerr << "Unknown data type: " << data_type << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error processing Parquet file: " << e.what() << std::endl;
    }
}

void ParquetProcessor::ProcessOHLCVData(const std::shared_ptr<arrow::Table>& table) {
    auto open_col = table->GetColumnByName("open");
    if (!open_col) open_col = table->GetColumnByName("Open");
    auto high_col = table->GetColumnByName("high");
    if (!high_col) high_col = table->GetColumnByName("High");
    auto low_col = table->GetColumnByName("low");
    if (!low_col) low_col = table->GetColumnByName("Low");
    auto close_col = table->GetColumnByName("close");
    if (!close_col) close_col = table->GetColumnByName("Close");
    auto volume_col = table->GetColumnByName("volume");
    if (!volume_col) volume_col = table->GetColumnByName("Volume");
    auto ticker_col = table->GetColumnByName("ticker");
    auto timestamp_col = table->GetColumnByName("ts_event");
    if (!timestamp_col) timestamp_col = table->GetColumnByName("Datetime");

    if (!open_col || !high_col || !low_col || !close_col || !volume_col || !ticker_col || !timestamp_col) {
        std::cerr << "One or more required columns are missing from the Parquet file." << std::endl;
        return;
    }

    auto close_array = std::static_pointer_cast<arrow::DoubleArray>(close_col->chunk(0));
    auto ticker_array = std::static_pointer_cast<arrow::StringArray>(ticker_col->chunk(0));
    auto timestamp_array = std::static_pointer_cast<arrow::TimestampArray>(timestamp_col->chunk(0));

    for (int64_t i = 0; i < table->num_rows(); ++i) {
        uint64_t price = static_cast<uint64_t>(close_array->Value(i) * 100); // Convert to cents
        uint64_t quantity = 1; // Dummy value
        std::string ticker = ticker_array->GetString(i);
        uint64_t timestamp = timestamp_array->Value(i);
        
        // Create a synthetic order book update
        order_client_->UpdateMarketState(ticker, timestamp);
        order_client_->SubmitOrder(i, true, quantity, price, timestamp, timestamp);
        order_client_->SubmitOrder(i, false, quantity, price + 1, timestamp, timestamp);
    }
}

void ParquetProcessor::ProcessMBP10Data(const std::shared_ptr<arrow::Table>& table) {
    auto ticker_col = table->GetColumnByName("ticker");
    auto timestamp_col = table->GetColumnByName("ts_event");

    if (!ticker_col || !timestamp_col) {
        std::cerr << "ticker or ts_event column is missing from the Parquet file." << std::endl;
        return;
    }

    auto ticker_array = std::static_pointer_cast<arrow::StringArray>(ticker_col->chunk(0));
    auto timestamp_array = std::static_pointer_cast<arrow::TimestampArray>(timestamp_col->chunk(0));

    for (int64_t i = 0; i < table->num_rows(); ++i) {
        std::string ticker = ticker_array->GetString(i);
        uint64_t timestamp = timestamp_array->Value(i);
        order_client_->UpdateMarketState(ticker, timestamp);

        for (int j = 0; j < 10; ++j) {
            std::string j_str = (j < 10 ? "0" : "") + std::to_string(j);
            auto bid_px_col = table->GetColumnByName("bid_px_" + j_str);
            auto ask_px_col = table->GetColumnByName("ask_px_" + j_str);
            auto bid_sz_col = table->GetColumnByName("bid_sz_" + j_str);
            auto ask_sz_col = table->GetColumnByName("ask_sz_" + j_str);

            if (bid_px_col && ask_px_col && bid_sz_col && ask_sz_col) {
                auto bid_px_array = std::static_pointer_cast<arrow::DoubleArray>(bid_px_col->chunk(0));
                auto ask_px_array = std::static_pointer_cast<arrow::DoubleArray>(ask_px_col->chunk(0));
                auto bid_sz_array = std::static_pointer_cast<arrow::DoubleArray>(bid_sz_col->chunk(0));
                auto ask_sz_array = std::static_pointer_cast<arrow::DoubleArray>(ask_sz_col->chunk(0));
                //TODO issues with casting
                double bid_price = bid_px_array->Value(i);
                double ask_price = ask_px_array->Value(i);
                double bid_sz_ar = bid_sz_array->Value(i);
                uint64_t bid_size = static_cast<uint64_t>(bid_sz_array->Value(i));
                uint64_t ask_size = static_cast<uint64_t>(ask_sz_array->Value(i));

                if (bid_size > 0) {
                    order_client_->SubmitOrder(i * 100 + j, true, bid_size, static_cast<uint64_t>(bid_price * 100), timestamp, timestamp);
                }
                if (ask_size > 0) {
                    order_client_->SubmitOrder(i * 100 + j + 10, false, ask_size, static_cast<uint64_t>(ask_price * 100), timestamp, timestamp);
                }
            }
        }
    }
}

void ParquetProcessor::ProcessMBP1Data(const std::shared_ptr<arrow::Table>& table) {
    auto ticker_col = table->GetColumnByName("ticker");
    auto timestamp_col = table->GetColumnByName("ts_event");
    auto bid_px_col = table->GetColumnByName("bid_px_00");
    auto ask_px_col = table->GetColumnByName("ask_px_00");
    auto bid_sz_col = table->GetColumnByName("bid_sz_00");
    auto ask_sz_col = table->GetColumnByName("ask_sz_00");

    if (!ticker_col || !timestamp_col || !bid_px_col || !ask_px_col || !bid_sz_col || !ask_sz_col) {
        std::cerr << "One or more required columns are missing from the Parquet file." << std::endl;
        return;
    }

    auto ticker_array = std::static_pointer_cast<arrow::StringArray>(ticker_col->chunk(0));
    auto timestamp_array = std::static_pointer_cast<arrow::TimestampArray>(timestamp_col->chunk(0));
    auto bid_px_array = std::static_pointer_cast<arrow::DoubleArray>(bid_px_col->chunk(0));
    auto ask_px_array = std::static_pointer_cast<arrow::DoubleArray>(ask_px_col->chunk(0));
    auto bid_sz_array = std::static_pointer_cast<arrow::UInt64Array>(bid_sz_col->chunk(0));
    auto ask_sz_array = std::static_pointer_cast<arrow::UInt64Array>(ask_sz_col->chunk(0));

    for (int64_t i = 0; i < table->num_rows(); ++i) {
        std::string ticker = ticker_array->GetString(i);
        uint64_t timestamp = timestamp_array->Value(i);
        order_client_->UpdateMarketState(ticker, timestamp);

        double bid_price = bid_px_array->Value(i);
        double ask_price = ask_px_array->Value(i);
        uint64_t bid_size = bid_sz_array->Value(i);
        uint64_t ask_size = ask_sz_array->Value(i);

        if (bid_size > 0) {
            order_client_->SubmitOrder(i * 100, true, bid_size, static_cast<uint64_t>(bid_price * 100), timestamp, timestamp);
        }
        if (ask_size > 0) {
            order_client_->SubmitOrder(i * 100 + 1, false, ask_size, static_cast<uint64_t>(ask_price * 100), timestamp, timestamp);
        }
    }
}