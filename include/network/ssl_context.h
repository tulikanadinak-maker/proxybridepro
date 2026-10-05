#pragma once

#include <boost/asio/ssl.hpp>
#include <string>
#include <memory>

namespace ProxyBridge {

namespace ssl = boost::asio::ssl;

class SslContextManager {
public:
    SslContextManager();
    ~SslContextManager();

    bool initialize();
    ssl::context& clientContext();
    ssl::context& serverContext();

private:
    std::unique_ptr<ssl::context> m_clientContext;
    std::unique_ptr<ssl::context> m_serverContext;
};

} // namespace ProxyBridge
