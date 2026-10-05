#pragma once

/**
 * @file proxy_server.h
 * @brief Backconnect proxy server
 *
 * Accepts client connections and routes outbound traffic through
 * rotating IPv6 addresses. Supports HTTP and SOCKS5 protocols.
 */

#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <cstdint>
#include <string>

namespace ProxyBridge {

class ConnectionManager;
class AsyncSocket;
class BackconnectProxy;
class HttpHandler;
class Socks5Handler;
class AuthManager;
class SubnetPool;
class SslContextManager;

struct ProxyConfig {
    std::string bindHost = "0.0.0.0";
    uint16_t bindPort = 1080;
    uint32_t maxConnections = 10000;
    uint32_t connectionTimeout = 30;
    bool enableHttp = true;
    bool enableSocks5 = true;
    bool requireAuth = true;
    std::string authPassword = "123456";
    uint32_t maxIpCount = 5;  // Max clients authenticated
};

class ProxyServer {
public:
    explicit ProxyServer(const ProxyConfig& config);
    ~ProxyServer();

    bool start(SubnetPool& pool);
    void stop();
    bool isRunning() const;

    const ProxyConfig& config() const;
    void setConfig(const ProxyConfig& config);

    ConnectionManager& connectionManager();
    AuthManager& authManager();
    uint16_t port() const;

private:
    void startAccept();
    void handleAccept(std::shared_ptr<AsyncSocket> socket,
                      const boost::system::error_code& error);
    void handleConnection(std::shared_ptr<AsyncSocket> clientSocket);
    void detectProtocol(std::shared_ptr<AsyncSocket> socket,
                        const std::vector<uint8_t>& data);

    ProxyConfig m_config;
    boost::asio::io_context m_ioContext;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> m_acceptor;
    std::unique_ptr<ConnectionManager> m_connectionManager;
    std::unique_ptr<SslContextManager> m_sslManager;
    std::unique_ptr<BackconnectProxy> m_backconnect;
    std::unique_ptr<HttpHandler> m_httpHandler;
    std::unique_ptr<Socks5Handler> m_socks5Handler;
    std::unique_ptr<AuthManager> m_authManager;
    SubnetPool* m_pool{nullptr};
    std::vector<std::thread> m_ioThreads;
    std::atomic<bool> m_running{false};
};

} // namespace ProxyBridge
