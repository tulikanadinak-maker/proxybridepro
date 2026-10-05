#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <chrono>
#include <functional>
#include <optional>
#include <deque>
#include <spdlog/spdlog.h>

namespace ProxyBridge {

enum class LogLevel { Trace, Debug, Info, Warning, Error, Critical };

struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::string message;
    std::string source;
};

using LogCallback = std::function<void(const LogEntry&)>;

class LogManager {
public:
    LogManager();
    ~LogManager();

    bool initialize(const std::string& logDir = "logs", size_t maxFileSize = 10*1024*1024, size_t maxFiles = 5);
    void log(LogLevel level, const std::string& message, const std::string& source = "");
    std::vector<LogEntry> getRecentLogs(size_t maxEntries = 500) const;
    void setLogLevel(LogLevel level);
    void setCallback(LogCallback callback);
    void clearBuffer();
    size_t bufferSize() const;

private:
    std::shared_ptr<spdlog::logger> m_logger;
    std::deque<LogEntry> m_buffer;
    mutable std::mutex m_mutex;
    LogCallback m_callback;
    LogLevel m_minLevel{LogLevel::Info};
    size_t m_maxBufferSize{5000};
    bool m_initialized{false};
};

} // namespace ProxyBridge
