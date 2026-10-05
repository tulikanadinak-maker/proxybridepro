#pragma once

/**
 * @file backconnect_proxy.h
 * @brief Backconnect proxy - binds outgoing connections to rotating IPv6
 *
 * When a client makes a request, this handler creates the outbound
 * connection bound to a specific IPv6 address from the pool.
 */

#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <functional>
#include <atomic>
#include <vector>
#include <cstdint>

namespace ProxyBridge {

class AsyncSocket;
class SubnetPool;
class ConnectionManager;

class BackconnectProxy {
public:
    BackconnectProxy(boost::asio::io_context& ioContext,
                     ConnectionManager& connManager,
                     SubnetPool& pool);
    ~BackconnectProxy();

    /**
     * @brief Create an outbound connection bound to next IPv6 from pool
     * @param targetHost Target to connect to
     * @param targetPort Target port
     * @param callback Called with the connected socket or error
     */
    void connectOutbound(const std::string& targetHost, uint16_t targetPort,
                         std::function<void(std::shared_ptr<AsyncSocket>, 
                                           const boost::system::error_code&)> callback);

    /**
     * @brief Start bidirectional tunnel between client and remote
     * @param initialData Optional buffered bytes already read from client
     *                    (e.g. SOCKS5 leftover) to flush to remote first.
     */
    void startTunnel(std::shared_ptr<AsyncSocket> client,
                     std::shared_ptr<AsyncSocket> remote,
                     std::shared_ptr<std::vector<uint8_t>> initialData = nullptr);

private:
    class TunnelSession;
    void connectWithRetry(const std::string& targetHost, uint16_t targetPort,
                          const std::string& bindAddr, int retryCount,
                          std::function<void(std::shared_ptr<AsyncSocket>,
                                           const boost::system::error_code&)> callback);

    boost::asio::io_context& m_ioContext;
    ConnectionManager& m_connManager;
    SubnetPool& m_pool;
};

} // namespace ProxyBridge
