#include "ipv6/ipv6_rotator.h"
#include "ipv6/ipv6_manager.h"

namespace ProxyBridge {

IPv6Rotator::IPv6Rotator(IPv6Manager& manager)
    : m_manager(manager) {}

IPv6Rotator::~IPv6Rotator() { stop(); }

void IPv6Rotator::setMode(RotationMode mode) { m_mode = mode; }
RotationMode IPv6Rotator::mode() const { return m_mode; }

void IPv6Rotator::setInterval(uint32_t seconds) { m_interval = seconds; }
uint32_t IPv6Rotator::interval() const { return m_interval; }

void IPv6Rotator::setRequestThreshold(uint32_t count) { m_requestThreshold = count; }
uint32_t IPv6Rotator::requestThreshold() const { return m_requestThreshold; }

void IPv6Rotator::start() {
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true)) return;

    if (m_mode == RotationMode::TimeBased) {
        m_timerThread = std::thread([this] { timerLoop(); });
    }
}

void IPv6Rotator::stop() {
    m_running = false;
    if (m_timerThread.joinable()) m_timerThread.join();
    m_requestCounter = 0;  // reset rotation state for a clean restart
}

bool IPv6Rotator::isRunning() const { return m_running; }

void IPv6Rotator::onRequest() {
    if (!m_running) return;

    if (m_mode == RotationMode::PerRequest) {
        rotateNow();
        return;
    }

    if (m_mode == RotationMode::RequestCount) {
        uint32_t count = m_requestCounter.fetch_add(1) + 1;
        if (count >= m_requestThreshold) {
            m_requestCounter = 0;
            rotateNow();
        }
    }
}

void IPv6Rotator::rotateNow() {
    // Serialize rotations with a per-rotator mutex, but do NOT hold the
    // IPv6Manager lock while the (slow, process-spawning) OS bind/unbind
    // calls run - rotateAll() itself is internally synchronized, and the
    // address swap happens under the manager's lock inside it.
    std::lock_guard<std::mutex> lock(m_mutex);
    m_manager.rotateAll();
}

void IPv6Rotator::resetNow() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_manager.resetAll();
}

void IPv6Rotator::timerLoop() {
    while (m_running) {
        auto endTime = std::chrono::steady_clock::now() +
                       std::chrono::seconds(m_interval.load());

        while (m_running && std::chrono::steady_clock::now() < endTime) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        if (m_running) {
            rotateNow();
        }
    }
}

} // namespace ProxyBridge
