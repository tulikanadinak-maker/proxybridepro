#pragma once

/**
 * @file ipv6_rotator.h
 * @brief Automatic IPv6 rotation strategies
 */

#include <atomic>
#include <thread>
#include <chrono>
#include <functional>
#include <mutex>
#include <cstdint>

namespace ProxyBridge {

class IPv6Manager;

enum class RotationMode : uint8_t {
    Manual,         // Only rotate on API call
    PerRequest,     // New IP every request
    TimeBased,      // Rotate every N seconds
    RequestCount    // Rotate every N requests
};

class IPv6Rotator {
public:
    explicit IPv6Rotator(IPv6Manager& manager);
    ~IPv6Rotator();

    void setMode(RotationMode mode);
    RotationMode mode() const;

    void setInterval(uint32_t seconds);
    uint32_t interval() const;

    void setRequestThreshold(uint32_t count);
    uint32_t requestThreshold() const;

    void start();
    void stop();
    bool isRunning() const;

    // Called on each request to check if rotation needed
    void onRequest();

    // Manual rotation trigger
    void rotateNow();
    void resetNow();

private:
    void timerLoop();

    IPv6Manager& m_manager;
    std::atomic<RotationMode> m_mode{RotationMode::Manual};
    std::atomic<uint32_t> m_interval{300};
    std::atomic<uint32_t> m_requestThreshold{100};
    std::atomic<uint32_t> m_requestCounter{0};
    std::atomic<bool> m_running{false};
    std::thread m_timerThread;
    std::mutex m_mutex;
};

} // namespace ProxyBridge
