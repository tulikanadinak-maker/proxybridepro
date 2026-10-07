#pragma once

/**
 * @file api_server.h
 * @brief REST API server for remote control
 *
 * Provides HTTP endpoints for:
 * - Dashboard info
 * - Rotate/Reset IP
 * - Status monitoring
 * - Configuration
 */

#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <cstdint>
#include <functional>
#include <unordered_map>

namespace ProxyBridge {

class AsyncSocket;

struct ApiConfig {
    std::string bindHost = "127.0.0.1";
    uint16_t port = 8089;
    std::string token = "123456";  // Auth token for API
    bool enabled = true;
};

using ApiHandler = std::function<std::string(const std::string& body, const std::string& query)>;

class ApiServer {
public:
    explicit ApiServer(const ApiConfig& config);
    ~ApiServer();

    bool start();
    void stop();
    bool isRunning() const;

    void setConfig(const ApiConfig& config);
    const ApiConfig& config() const;

    // Register route handlers
    void registerRoute(const std::string& method, const std::string& path, ApiHandler handler);

private:
    void startAccept();
    void handleConnection(std::shared_ptr<AsyncSocket> client);
    void readMore(std::shared_ptr<AsyncSocket> client,
                  std::shared_ptr<std::string> buffer,
                  std::shared_ptr<boost::asio::steady_timer> timer);
    void processRequest(std::shared_ptr<AsyncSocket> client, const std::string& raw);
    void sendResponse(std::shared_ptr<AsyncSocket> client, int code,
                      const std::string& contentType, const std::string& body);
    bool validateToken(const std::string& rawRequest);

    ApiConfig m_config;
    boost::asio::io_context m_ioContext;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> m_acceptor;
    std::unordered_map<std::string, ApiHandler> m_routes; // "GET /path" -> handler
    std::vector<std::thread> m_threads;
    std::atomic<bool> m_running{false};
};

} // namespace ProxyBridge
