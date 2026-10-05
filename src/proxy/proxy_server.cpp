#include "proxy/proxy_server.h"
#include "network/connection_manager.h"
#include "network/async_socket.h"
#include "network/ssl_context.h"
#include "proxy/backconnect_proxy.h"
#include "proxy/http_handler.h"
#include "proxy/socks5_handler.h"
#include "proxy/auth_manager.h"
#include "ipv6/subnet_pool.h"

namespace ProxyBridge {

ProxyServer::ProxyServer(const ProxyConfig& config) : m_config(config) {}
ProxyServer::~ProxyServer() { stop(); }

bool ProxyServer::start(SubnetPool& pool) {
    if (m_running) return true;
    m_pool = &pool;

    try {
        m_sslManager = std::make_unique<SslContextManager>();
        m_sslManager->initialize();

        m_connectionManager = std::make_unique<ConnectionManager>(m_ioContext, m_config.maxConnections);
        m_connectionManager->initialize();

        m_authManager = std::make_unique<AuthManager>();
        m_authManager->setPassword(m_config.authPassword);
        m_authManager->setMaxIpCount(m_config.maxIpCount);
        m_authManager->setEnabled(m_config.requireAuth);

        m_backconnect = std::make_unique<BackconnectProxy>(m_ioContext, *m_connectionManager, *m_pool);
        m_httpHandler = std::make_unique<HttpHandler>(m_ioContext, *m_backconnect, *m_authManager);
        m_socks5Handler = std::make_unique<Socks5Handler>(m_ioContext, *m_backconnect, *m_authManager);

        auto ep = boost::asio::ip::tcp::endpoint(
            boost::asio::ip::make_address(m_config.bindHost), m_config.bindPort);
        m_acceptor = std::make_unique<boost::asio::ip::tcp::acceptor>(m_ioContext);
        m_acceptor->open(ep.protocol());
        m_acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        m_acceptor->bind(ep);
        m_acceptor->listen(boost::asio::socket_base::max_listen_connections);

        m_running = true;
        startAccept();

        uint32_t threads = std::thread::hardware_concurrency();
        if (threads == 0) threads = 4;
        for (uint32_t i = 0; i < threads; ++i)
            m_ioThreads.emplace_back([this] { m_ioContext.run(); });

        return true;
    } catch (const std::exception& e) {
        m_running = false;
        // Log will be visible if LogManager is available through Application
        return false;
    }
}

void ProxyServer::stop() {
    if (!m_running) return;
    m_running = false;
    if (m_acceptor) { boost::system::error_code ec; m_acceptor->close(ec); }
    if (m_connectionManager) m_connectionManager->closeAll();
    m_ioContext.stop();
    for (auto& t : m_ioThreads) if (t.joinable()) t.join();
    m_ioThreads.clear();
    m_ioContext.restart();
}

bool ProxyServer::isRunning() const { return m_running; }
const ProxyConfig& ProxyServer::config() const { return m_config; }
void ProxyServer::setConfig(const ProxyConfig& config) { m_config = config; }
ConnectionManager& ProxyServer::connectionManager() { return *m_connectionManager; }
AuthManager& ProxyServer::authManager() { return *m_authManager; }
uint16_t ProxyServer::port() const { return m_config.bindPort; }

void ProxyServer::startAccept() {
    if (!m_running) return;
    auto socket = std::make_shared<AsyncSocket>(m_ioContext);
    m_acceptor->async_accept(socket->socket(),
        [this, socket](const boost::system::error_code& ec) { handleAccept(socket, ec); });
}

void ProxyServer::handleAccept(std::shared_ptr<AsyncSocket> socket,
                                const boost::system::error_code& error) {
    if (!m_running) return;
    if (error) {
        // Accept failed: log and (unless shutting down) keep accepting.
        if (error != boost::asio::error::operation_aborted) {
            startAccept();
        }
        return;
    }
    handleConnection(socket);
    startAccept();
}

void ProxyServer::handleConnection(std::shared_ptr<AsyncSocket> clientSocket) {
    uint64_t connId = m_connectionManager->registerConnection(clientSocket);
    if (connId == 0) { clientSocket->close(); return; }

    auto buffer = std::make_shared<std::vector<uint8_t>>(4096);
    clientSocket->asyncRead(boost::asio::buffer(*buffer),
        [this, clientSocket, buffer, connId](auto ec, size_t bytesRead) {
            if (ec) { m_connectionManager->removeConnection(connId); return; }
            buffer->resize(bytesRead);
            detectProtocol(clientSocket, *buffer);
        });
}

void ProxyServer::detectProtocol(std::shared_ptr<AsyncSocket> socket,
                                  const std::vector<uint8_t>& data) {
    if (data.empty()) { socket->close(); return; }

    if (data[0] == 0x05 && m_config.enableSocks5) {
        m_socks5Handler->handleConnection(socket, data);
    } else if (data[0] >= 'A' && data[0] <= 'Z' && m_config.enableHttp) {
        m_httpHandler->handleConnection(socket, data);
    } else {
        socket->close();
    }
}

} // namespace ProxyBridge
