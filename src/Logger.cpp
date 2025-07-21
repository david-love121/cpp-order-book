#include "Logger.h"

std::shared_ptr<Logger> GLogger = std::make_shared<Logger>();

Logger::Logger() : m_output_stream(&std::cout) {}

Logger::~Logger() {
    if (m_file_stream.is_open()) {
        m_file_stream.close();
    }
}

void Logger::set_log_file(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file_stream.is_open()) {
        m_file_stream.close();
    }
    m_file_stream.open(path, std::ios::out | std::ios::trunc);
    if (m_file_stream.is_open()) {
        m_output_stream = &m_file_stream;
    } else {
        m_output_stream = &std::cout;
    }
}

Logger& Logger::operator<<(std::ostream& (*manip)(std::ostream&)) {
    std::lock_guard<std::mutex> lock(m_mutex);
    *m_output_stream << manip;
    return *this;
}