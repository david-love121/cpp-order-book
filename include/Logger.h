#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>

class Logger {
public:
    Logger();
    ~Logger();

    void set_log_file(const std::string& path);

    template<typename T>
    Logger& operator<<(const T& msg) {
        std::lock_guard<std::mutex> lock(m_mutex);
        *m_output_stream << msg;
        return *this;
    }

    // Handle std::endl and other manipulators
    Logger& operator<<(std::ostream& (*manip)(std::ostream&));

private:
    std::ofstream m_file_stream;
    std::ostream* m_output_stream;
    std::mutex m_mutex;
};

// Global logger instance
extern std::shared_ptr<Logger> GLogger;

#endif // LOGGER_H