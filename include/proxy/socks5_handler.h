#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace ProxyBridge {

class AsyncSocket;
class BackconnectProxy;
class AuthManager;

class Socks5Handler {
public:
    Socks5Handler(boost::asio::io_context& ioContext,
                  BackconnectProxy& backconnect,
                  AuthManager& authManager);
    ~Socks5Handler();

    void handleConnection(std::shared_ptr<AsyncSocket> client,
                          const std::vector<uint8_t>& initialData);

private:
    void handleGreeting(std::shared_ptr<AsyncSocket> client,
                        const std::vector<uint8_t>& data);
    void handleAuth(std::shared_ptr<AsyncSocket> client,
                    const std::shared_ptr<std::vector<uint8_t>>& leftover);
    void processAuth(std::shared_ptr<AsyncSocket> client,
                     const std::string& password,
                     std::shared_ptr<std::vector<uint8_t>> leftover);
    void sendAuthReply(std::shared_ptr<AsyncSocket> client, bool success,
                       std::shared_ptr<std::vector<uint8_t>> leftover);
    void handleRequest(std::shared_ptr<AsyncSocket> client,
                       const std::shared_ptr<std::vector<uint8_t>>& leftover);
    void sendErrorReply(std::shared_ptr<AsyncSocket> client, uint8_t reply);
    void connectTarget(std::shared_ptr<AsyncSocket> client,
                       const std::string& host, uint16_t port,
                       std::shared_ptr<std::vector<uint8_t>> leftover);
    void sendMethodSelection(std::shared_ptr<AsyncSocket> client, uint8_t method);
    void sendReply(std::shared_ptr<AsyncSocket> client, uint8_t reply,
                   const std::string& bindAddr, uint16_t bindPort);

    boost::asio::io_context& m_ioContext;
    BackconnectProxy& m_backconnect;
    AuthManager& m_authManager;
};

} // namespace ProxyBridge
