#include "network/connection_manager.h"
#include "network/async_socket.h"

namespace ProxyBridge {

ConnectionManager::ConnectionManager(boost::asio::io_context& ioContext, uint32_t maxConnections)
    : m_ioContext(ioContext), m_statsTimer(ioContext), m_maxConnections(maxConnections) {}

ConnectionManager::~ConnectionManager() {
    // Stop the stats timer BEFORE members are freed (avoid dangling this in
    // any pending async_wait callback).
    m_statsTimer.cancel();
    closeAll();
}

bool ConnectionManager::initialize() { updateSpeedStats(); return true; }

uint64_t ConnectionManager::registerConnection(std::shared_ptr<AsyncSocket> socket) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Limit check is inside the mutex to avoid a race between the check and
    // the insertion (two threads could both pass the check).
    if (m_stats.activeConnections >= m_maxConnections) return 0;
    uint64_t id = m_nextId++;
    m_connections[id] = socket;
    m_socketIds[socket.get()] = id;
    ConnectionInfo info; info.id = id;
    info.connectedAt = std::chrono::system_clock::now();
    std::string remote = socket->remoteEndpoint();
    auto pos = remote.rfind(':');
    if (pos != std::string::npos) {
        info.remoteAddress = remote.substr(0, pos);
        try {
            info.remotePort = static_cast<uint16_t>(std::stoi(remote.substr(pos + 1)));
        } catch (...) {
            info.remotePort = 0;
        }
    }
    m_connectionInfo[id] = info;
    m_stats.totalConnections++; m_stats.activeConnections++;
    return id;
}

void ConnectionManager::removeConnection(uint64_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_connections.find(id);
    if (it != m_connections.end()) {
        m_socketIds.erase(it->second.get());
        it->second->close();
        m_connections.erase(it);
        m_connectionInfo.erase(id);
        // Only decrement for connections that actually existed (no underflow).
        m_stats.activeConnections--;
    }
}

void ConnectionManager::releaseBySocket(AsyncSocket* socket) {
    if (!socket) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_socketIds.find(socket);
    if (it == m_socketIds.end()) return;
    uint64_t id = it->second;
    m_socketIds.erase(it);
    m_connections.erase(id);
    m_connectionInfo.erase(id);
    if (m_stats.activeConnections > 0) m_stats.activeConnections--;
}

void ConnectionManager::recordTransfer(uint64_t /*id*/, uint64_t bytesIn, uint64_t bytesOut) {
    m_stats.totalBytesIn += bytesIn; m_stats.totalBytesOut += bytesOut;
}

void ConnectionManager::recordSuccess() { m_stats.successfulRequests++; }
void ConnectionManager::recordFailure() { m_stats.failedRequests++; }
uint64_t ConnectionManager::activeCount() const { return m_stats.activeConnections; }
const ConnectionStats& ConnectionManager::stats() const { return m_stats; }

std::vector<ConnectionInfo> ConnectionManager::getActiveConnections() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ConnectionInfo> result;
    for (const auto& [id, info] : m_connectionInfo) result.push_back(info);
    return result;
}

void ConnectionManager::closeAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [id, s] : m_connections) s->close();
    m_connections.clear(); m_socketIds.clear(); m_connectionInfo.clear(); m_stats.activeConnections = 0;
}

void ConnectionManager::setMaxConnections(uint32_t max) { m_maxConnections = max; }
boost::asio::io_context& ConnectionManager::ioContext() { return m_ioContext; }

void ConnectionManager::updateSpeedStats() {
    uint64_t curIn = m_stats.totalBytesIn, curOut = m_stats.totalBytesOut;
    m_stats.bytesInPerSecond = curIn - m_lastBytesIn;
    m_stats.bytesOutPerSecond = curOut - m_lastBytesOut;
    m_lastBytesIn = curIn; m_lastBytesOut = curOut;
    m_statsTimer.expires_after(std::chrono::seconds(1));
    m_statsTimer.async_wait([this](auto ec) { if (!ec) updateSpeedStats(); });
}

} // namespace ProxyBridge
