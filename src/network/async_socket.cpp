#include "network/async_socket.h"

namespace ProxyBridge {

AsyncSocket::AsyncSocket(asio::io_context& ioContext)
    : m_ioContext(ioContext), m_socket(std::make_unique<tcp::socket>(ioContext)),
      m_timer(ioContext), m_isSsl(false) {}

AsyncSocket::AsyncSocket(asio::io_context& ioContext, ssl::context& sslContext)
    : m_ioContext(ioContext),
      m_sslSocket(std::make_unique<ssl::stream<tcp::socket>>(ioContext, sslContext)),
      m_timer(ioContext), m_isSsl(true) {}

AsyncSocket::~AsyncSocket() { close(); }

void AsyncSocket::asyncConnect(const std::string& host, uint16_t port,
                               ConnectCallback callback, uint32_t timeoutMs) {
    auto self = shared_from_this();
    m_timer.expires_after(std::chrono::milliseconds(timeoutMs));
    m_timer.async_wait([self](const boost::system::error_code& ec) {
        if (!ec) self->close();
    });

    auto resolver = std::make_shared<tcp::resolver>(m_ioContext);
    resolver->async_resolve(host, std::to_string(port),
        [self, resolver, callback](const boost::system::error_code& ec,
                                    tcp::resolver::results_type results) {
            if (ec) { self->m_timer.cancel(); callback(ec); return; }
            auto& sock = self->m_isSsl ? self->m_sslSocket->lowest_layer() : *self->m_socket;
            asio::async_connect(sock, results,
                [self, callback](const boost::system::error_code& ec, const tcp::endpoint&) {
                    self->m_timer.cancel();
                    if (!ec) self->m_connected = true;
                    callback(ec);
                });
        });
}

void AsyncSocket::asyncConnectBind(const std::string& host, uint16_t port,
                                    const std::string& localAddr,
                                    ConnectCallback callback, uint32_t timeoutMs) {
    auto self = shared_from_this();
    m_timer.expires_after(std::chrono::milliseconds(timeoutMs));
    m_timer.async_wait([self](const boost::system::error_code& ec) {
        if (!ec) self->close();
    });

    // Bind to local IPv6 address first
    auto& sock = m_isSsl ? m_sslSocket->lowest_layer() : *m_socket;
    boost::system::error_code bindEc;
    auto localEp = tcp::endpoint(boost::asio::ip::make_address(localAddr, bindEc), 0);
    if (bindEc) { callback(bindEc); return; }

    sock.open(localEp.protocol(), bindEc);
    if (bindEc) { callback(bindEc); return; }

    sock.bind(localEp, bindEc);
    if (bindEc) { callback(bindEc); return; }

    auto resolver = std::make_shared<tcp::resolver>(m_ioContext);
    resolver->async_resolve(host, std::to_string(port),
        [self, resolver, callback](const boost::system::error_code& ec,
                                    tcp::resolver::results_type results) {
            if (ec) { self->m_timer.cancel(); callback(ec); return; }
            auto& sock = self->m_isSsl ? self->m_sslSocket->lowest_layer() : *self->m_socket;
            asio::async_connect(sock, results,
                [self, callback](const boost::system::error_code& ec, const tcp::endpoint&) {
                    self->m_timer.cancel();
                    if (!ec) self->m_connected = true;
                    callback(ec);
                });
        });
}

void AsyncSocket::asyncRead(asio::mutable_buffer buffer, ReadCallback callback) {
    auto self = shared_from_this();
    if (m_isSsl) {
        m_sslSocket->async_read_some(buffer, [self, callback](auto ec, size_t b) { callback(ec, b); });
    } else {
        m_socket->async_read_some(buffer, [self, callback](auto ec, size_t b) { callback(ec, b); });
    }
}

void AsyncSocket::asyncReadUntil(asio::streambuf& buffer, const std::string& delim, ReadCallback callback) {
    auto self = shared_from_this();
    if (m_isSsl) {
        asio::async_read_until(*m_sslSocket, buffer, delim, [self, callback](auto ec, size_t b) { callback(ec, b); });
    } else {
        asio::async_read_until(*m_socket, buffer, delim, [self, callback](auto ec, size_t b) { callback(ec, b); });
    }
}

void AsyncSocket::asyncWrite(const std::vector<uint8_t>& data, WriteCallback callback) {
    auto self = shared_from_this();
    auto buf = std::make_shared<std::vector<uint8_t>>(data);
    if (m_isSsl) {
        asio::async_write(*m_sslSocket, asio::buffer(*buf), [self, buf, callback](auto ec, size_t b) { callback(ec, b); });
    } else {
        asio::async_write(*m_socket, asio::buffer(*buf), [self, buf, callback](auto ec, size_t b) { callback(ec, b); });
    }
}

void AsyncSocket::asyncWrite(const std::string& data, WriteCallback callback) {
    auto self = shared_from_this();
    auto buf = std::make_shared<std::string>(data);
    if (m_isSsl) {
        asio::async_write(*m_sslSocket, asio::buffer(*buf), [self, buf, callback](auto ec, size_t b) { callback(ec, b); });
    } else {
        asio::async_write(*m_socket, asio::buffer(*buf), [self, buf, callback](auto ec, size_t b) { callback(ec, b); });
    }
}

void AsyncSocket::asyncHandshake(ssl::stream_base::handshake_type type, ConnectCallback callback) {
    if (!m_isSsl) { callback({}); return; }
    auto self = shared_from_this();
    m_sslSocket->async_handshake(type, [self, callback](auto ec) { callback(ec); });
}

void AsyncSocket::close() {
    boost::system::error_code ec;
    m_timer.cancel();
    if (m_isSsl && m_sslSocket) m_sslSocket->lowest_layer().close(ec);
    else if (m_socket) m_socket->close(ec);
    m_connected = false;
}

bool AsyncSocket::isOpen() const {
    if (m_isSsl && m_sslSocket) return m_sslSocket->lowest_layer().is_open();
    return m_socket && m_socket->is_open();
}

bool AsyncSocket::isSsl() const { return m_isSsl; }

tcp::socket& AsyncSocket::socket() {
    if (m_isSsl) return static_cast<tcp::socket&>(m_sslSocket->lowest_layer());
    return *m_socket;
}

std::string AsyncSocket::remoteEndpoint() const {
    try {
        boost::system::error_code ec;
        auto ep = m_isSsl ? m_sslSocket->lowest_layer().remote_endpoint(ec) : m_socket->remote_endpoint(ec);
        if (!ec) return ep.address().to_string() + ":" + std::to_string(ep.port());
    } catch (...) {}
    return "";
}

std::string AsyncSocket::localEndpoint() const {
    try {
        boost::system::error_code ec;
        auto ep = m_isSsl ? m_sslSocket->lowest_layer().local_endpoint(ec) : m_socket->local_endpoint(ec);
        if (!ec) return ep.address().to_string() + ":" + std::to_string(ep.port());
    } catch (...) {}
    return "";
}

} // namespace ProxyBridge
