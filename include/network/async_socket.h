#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <cstdint>

namespace ProxyBridge {

using boost::asio::ip::tcp;
namespace asio = boost::asio;
namespace ssl = boost::asio::ssl;

using ConnectCallback = std::function<void(const boost::system::error_code&)>;
using ReadCallback = std::function<void(const boost::system::error_code&, size_t)>;
using WriteCallback = std::function<void(const boost::system::error_code&, size_t)>;

class AsyncSocket : public std::enable_shared_from_this<AsyncSocket> {
public:
    explicit AsyncSocket(asio::io_context& ioContext);
    AsyncSocket(asio::io_context& ioContext, ssl::context& sslContext);
    ~AsyncSocket();

    void asyncConnect(const std::string& host, uint16_t port,
                      ConnectCallback callback, uint32_t timeoutMs = 10000);

    // Connect with specific local bind address (for IPv6 rotation)
    void asyncConnectBind(const std::string& host, uint16_t port,
                          const std::string& localAddr,
                          ConnectCallback callback, uint32_t timeoutMs = 10000);

    void asyncRead(asio::mutable_buffer buffer, ReadCallback callback);
    void asyncReadUntil(asio::streambuf& buffer, const std::string& delim, ReadCallback callback);
    void asyncWrite(const std::vector<uint8_t>& data, WriteCallback callback);
    void asyncWrite(const std::string& data, WriteCallback callback);
    void asyncHandshake(ssl::stream_base::handshake_type type, ConnectCallback callback);

    void close();
    bool isOpen() const;
    bool isSsl() const;
    tcp::socket& socket();
    std::string remoteEndpoint() const;
    std::string localEndpoint() const;

private:
    asio::io_context& m_ioContext;
    std::unique_ptr<tcp::socket> m_socket;
    std::unique_ptr<ssl::stream<tcp::socket>> m_sslSocket;
    asio::steady_timer m_timer;
    bool m_isSsl;
    bool m_connected{false};
};

} // namespace ProxyBridge
