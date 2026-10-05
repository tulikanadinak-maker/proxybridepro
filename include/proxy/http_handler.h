#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace ProxyBridge {

class AsyncSocket;
class BackconnectProxy;
class AuthManager;

struct HttpRequest {
    std::string method;
    std::string url;
    std::string version;
    std::string host;
    uint16_t port = 80;
    std::unordered_map<std::string, std::string> headers;
    bool isConnect = false;
};

class HttpHandler {
public:
    HttpHandler(boost::asio::io_context& ioContext,
                BackconnectProxy& backconnect,
                AuthManager& authManager);
    ~HttpHandler();

    void handleConnection(std::shared_ptr<AsyncSocket> client,
                          const std::vector<uint8_t>& initialData);

private:
    HttpRequest parseRequest(const std::string& raw);
    bool authenticate(const HttpRequest& request, std::shared_ptr<AsyncSocket> client);
    void handleConnect(std::shared_ptr<AsyncSocket> client, const HttpRequest& req);
    void handleRelay(std::shared_ptr<AsyncSocket> client, const HttpRequest& req,
                     const std::string& rawRequest);
    void sendError(std::shared_ptr<AsyncSocket> client, int code, const std::string& msg);

    boost::asio::io_context& m_ioContext;
    BackconnectProxy& m_backconnect;
    AuthManager& m_authManager;
};

} // namespace ProxyBridge
