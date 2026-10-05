#include "log/log_manager.h"
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <algorithm>
#include <filesystem>

namespace ProxyBridge {

namespace {
spdlog::level::level_enum toSpdlog(LogLevel l) {
    switch (l) {
        case LogLevel::Trace: return spdlog::level::trace;
        case LogLevel::Debug: return spdlog::level::debug;
        case LogLevel::Info: return spdlog::level::info;
        case LogLevel::Warning: return spdlog::level::warn;
        case LogLevel::Error: return spdlog::level::err;
        case LogLevel::Critical: return spdlog::level::critical;
    }
    return spdlog::level::info;
}
}

LogManager::LogManager() = default;
LogManager::~LogManager() { if (m_logger) m_logger->flush(); }

bool LogManager::initialize(const std::string& logDir, size_t maxFileSize, size_t maxFiles) {
    try {
        std::filesystem::create_directories(logDir);
        auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            logDir + "/proxybridge.log", maxFileSize, maxFiles);
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        m_logger = std::make_shared<spdlog::logger>("PBP",
            spdlog::sinks_init_list{fileSink, consoleSink});
        m_logger->set_level(spdlog::level::debug);
        m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        m_logger->flush_on(spdlog::level::warn);
        m_initialized = true;
        return true;
    } catch (...) { return false; }
}

void LogManager::log(LogLevel level, const std::string& message, const std::string& source) {
    if (!m_initialized || level < m_minLevel) return;

    std::string formatted = source.empty() ? message : "[" + source + "] " + message;
    m_logger->log(toSpdlog(level), formatted);

    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.level = level;
    entry.message = message;
    entry.source = source;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_buffer.push_back(entry);
        if (m_buffer.size() > m_maxBufferSize) m_buffer.pop_front();
    }
    if (m_callback) m_callback(entry);
}

std::vector<LogEntry> LogManager::getRecentLogs(size_t maxEntries) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<LogEntry> result;
    size_t start = (m_buffer.size() > maxEntries) ? m_buffer.size() - maxEntries : 0;
    for (size_t i = start; i < m_buffer.size(); ++i) result.push_back(m_buffer[i]);
    return result;
}

void LogManager::setLogLevel(LogLevel level) {
    m_minLevel = level;
    if (m_logger) m_logger->set_level(toSpdlog(level));
}

void LogManager::setCallback(LogCallback callback) { m_callback = std::move(callback); }
void LogManager::clearBuffer() { std::lock_guard<std::mutex> lock(m_mutex); m_buffer.clear(); }
size_t LogManager::bufferSize() const { std::lock_guard<std::mutex> lock(m_mutex); return m_buffer.size(); }

} // namespace ProxyBridge
