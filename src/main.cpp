#include <iostream>
#include <vector>

#include <chrono>
#include <thread>


#include <memory>
#include <cstdlib>
#include <cstring>
#include <fstream>


// Include the OrderBook headers

#include "DatabentoCache.h"

#include "OrderBookManager.h"
#include "DatabentoProcessor.h"
#include "IndicatorLogger.h"
#include "Logger.h"

// Include Databento headers
#include <databento/live.hpp>
#include <databento/historical.hpp>
#include <databento/symbol_map.hpp>
#include <databento/dbn.hpp>
#include <databento/dbn_file_store.hpp>

using namespace databento;

// Function to load environment variables from .env file
void loadEnvFile(const std::string& filename = ".env") {
    std::vector<std::string> search_paths = {
        filename,                                           // Current working directory
        "../" + filename,                                   // Parent directory (for build/)
        "../../" + filename,                               // For nested build directories
        std::string(__FILE__).substr(0, std::string(__FILE__).find_last_of('/')) + "/../" + filename  // Relative to source file
    };
    
    std::ifstream file;
    std::string found_path;
    
    for (const auto& path : search_paths) {
        file.open(path);
        if (file.is_open()) {
            found_path = path;
            *GLogger << "[ENV] Found .env file at: " << path << '\n';
            break;
        }
    }
    
    if (!file.is_open()) {
        *GLogger << "[ENV] .env file not found in any of these locations:" << '\n';
        for (const auto& path : search_paths) {
            *GLogger << "[ENV]   " << path << '\n';
        }
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        
        // Find the = separator
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;
        
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        
        // Remove leading/trailing whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        // Set environment variable
        if (setenv(key.c_str(), value.c_str(), 1) == 0) {
            *GLogger << "[ENV] Loaded: " << key << '\n';
        }
    }
    int x = 2;
    file.close();
}
void printHelp() {
    *GLogger << "Usage: cpp_consumer_fetchcontent\n"
              << "Options:\n"
              << "  --help                         Show this help message\n";
}

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--help") {
        printHelp();
        return 0;
    }

    *GLogger << "OrderBook C++ Engine" << '\n';
    *GLogger << "====================" << '\n';
    *GLogger << "This executable is the core engine. It is meant to be controlled by a Python script." << '\n';
    
    return 0;
}
