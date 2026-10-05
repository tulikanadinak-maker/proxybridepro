#pragma once

#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <vector>
#include <chrono>
#include <cstdint>
#include <boost/asio.hpp>

namespace ProxyBridge {

class AsyncSocket;

struct ConnectionStats {
    std::atomic<uint64_t> totalConnections{0};
    std::atomic<uint64_t> activeConnections{0};
    std::atomic<uint64_t> totalBytesIn{0};
    std::atomic<uint64_t> totalBytesOut{0};
    std::atomic<uint64_t> successfulRequests{0};
    std::atomic<uint64_t> failedRequests{0};
    std::atomic<uint64_t> bytesInPerSecond{0};
    std::atomic<uint64_t> bytesOutPerSecond{0};
};

struct ConnectionInfo {
    uint64_t id;
    std::string remoteAddress;
    uint16_t remotePort;
    std::chrono::system_clock::time_point connectedAt;
    uint64_t bytesIn = 0;
    uint64_t bytesOut = 0;
};

class ConnectionManager {
public:
    explicit ConnectionManager(boost::asio::io_context& ioContext, uint32_t maxConnections = 10000);
    ~ConnectionManager();

    bool initialize();
    uint64_t registerConnection(std::shared_ptr<AsyncSocket> socket);
    void removeConnection(uint64_t id);
    /** Unregister the connection tied to a socket (used by tunnels that no
     *  longer carry the numeric id). Safe to call more than once. */
    void releaseBySocket(AsyncSocket* socket);
    void recordTransfer(uint64_t id, uint64_t bytesIn, uint64_t bytesOut);
    void recordSuccess();
    void recordFailure();
    uint64_t activeCount() const;
    const ConnectionStats& stats() const;
    std::vector<ConnectionInfo> getActiveConnections() const;
    void closeAll();
    void setMaxConnections(uint32_t max);
    boost::asio::io_context& ioContext();

private:
    void updateSpeedStats();

    boost::asio::io_context& m_ioContext;
    boost::asio::steady_timer m_statsTimer;
    std::unordered_map<uint64_t, std::shared_ptr<AsyncSocket>> m_connections;
    std::unordered_map<AsyncSocket*, uint64_t> m_socketIds;
    std::unordered_map<uint64_t, ConnectionInfo> m_connectionInfo;
    ConnectionStats m_stats;
    mutable std::mutex m_mutex;
    std::atomic<uint64_t> m_nextId{1};
    std::atomic<uint32_t> m_maxConnections;
    uint64_t m_lastBytesIn{0};
    uint64_t m_lastBytesOut{0};
};

} // namespace ProxyBridge
