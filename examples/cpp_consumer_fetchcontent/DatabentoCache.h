#pragma once

#include <string>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <vector>
#include <databento/dbn.hpp>
#include <databento/historical.hpp>

namespace fs = std::filesystem;
using namespace databento;

/**
 * Cache manager for Databento historical data
 * Stores data locally to avoid repeated API calls during development/debugging
 */
class DatabentoCache {
public:
    DatabentoCache(const std::string& cache_dir = "databento_cache") 
        : cache_directory_(cache_dir) {
        // Create cache directory if it doesn't exist
        fs::create_directories(cache_directory_);
    }

    /**
     * Generate a cache filename based on query parameters
     */
    std::string generateCacheKey(const std::string& dataset,
                                const std::string& start_time,
                                const std::string& end_time,
                                const std::vector<std::string>& symbols,
                                Schema schema) {
        std::string key = dataset + "_" + start_time + "_" + end_time + "_";
        for (const auto& symbol : symbols) {
            key += symbol + "_";
        }
        key += std::to_string(static_cast<int>(schema));
        
        // Replace problematic characters for filename
        std::replace(key.begin(), key.end(), ':', '-');
        std::replace(key.begin(), key.end(), 'T', '_');
        
        return key + ".dbn";
    }

    /**
     * Check if cached data exists
     */
    bool hasCachedData(const std::string& cache_key) {
        fs::path cache_file = cache_directory_ / cache_key;
        
        if (!fs::exists(cache_file)) {
            return false;
        }
        
        std::cout << "[CACHE] Found cached data: " << cache_key << std::endl;
        return true;
    }

    /**
     * Save data to cache file
     */
    void saveToCache(const std::string& cache_key, const std::vector<uint8_t>& data) {
        fs::path cache_file = cache_directory_ / cache_key;
        
        std::cout << "[CACHE] Saving data to cache: " << cache_file.string() 
                  << " (" << data.size() << " bytes)" << std::endl;
        
        std::ofstream file(cache_file, std::ios::binary);
        if (!file) {
            std::cerr << "[CACHE] Failed to open cache file for writing: " << cache_file << std::endl;
            return;
        }
        
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.close();
        
        if (file.good()) {
            std::cout << "[CACHE] Successfully cached data" << std::endl;
        } else {
            std::cerr << "[CACHE] Error writing to cache file" << std::endl;
        }
    }

    /**
     * Load data from cache file
     */
    std::vector<uint8_t> loadFromCache(const std::string& cache_key) {
        fs::path cache_file = cache_directory_ / cache_key;
        
        if (!fs::exists(cache_file)) {
            return {};
        }
        
        std::cout << "[CACHE] Loading data from cache: " << cache_file.string() << std::endl;
        
        std::ifstream file(cache_file, std::ios::binary | std::ios::ate);
        if (!file) {
            std::cerr << "[CACHE] Failed to open cache file for reading: " << cache_file << std::endl;
            return {};
        }
        
        auto file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> data(file_size);
        file.read(reinterpret_cast<char*>(data.data()), file_size);
        
        if (file.good()) {
            std::cout << "[CACHE] Successfully loaded " << data.size() << " bytes from cache" << std::endl;
        } else {
            std::cerr << "[CACHE] Error reading from cache file" << std::endl;
            return {};
        }
        
        return data;
    }

    /**
     * Get cache file path for inspection
     */
    std::string getCacheFilePath(const std::string& cache_key) {
        fs::path cache_file = cache_directory_ / cache_key;
        return cache_file.string();
    }

    /**
     * Clear all cached data
     */
    void clearCache() {
        std::cout << "[CACHE] Clearing cache directory: " << cache_directory_ << std::endl;
        
        try {
            for (const auto& entry : fs::directory_iterator(cache_directory_)) {
                if (entry.is_regular_file() && entry.path().extension() == ".dbn") {
                    fs::remove(entry);
                    std::cout << "[CACHE] Removed: " << entry.path().filename() << std::endl;
                }
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "[CACHE] Error clearing cache: " << e.what() << std::endl;
        }
    }

    /**
     * List cached files
     */
    void listCache() {
        std::cout << "[CACHE] Cached files in " << cache_directory_ << ":" << std::endl;
        
        try {
            bool found_any = false;
            for (const auto& entry : fs::directory_iterator(cache_directory_)) {
                if (entry.is_regular_file() && entry.path().extension() == ".dbn") {
                    auto file_size = fs::file_size(entry);
                    auto file_time = fs::last_write_time(entry);
                    
                    std::cout << "  " << entry.path().filename().string() 
                              << " (" << file_size << " bytes)" << std::endl;
                    found_any = true;
                }
            }
            
            if (!found_any) {
                std::cout << "  (no cached files found)" << std::endl;
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "[CACHE] Error listing cache: " << e.what() << std::endl;
        }
    }

private:
    std::filesystem::path cache_directory_;
};
