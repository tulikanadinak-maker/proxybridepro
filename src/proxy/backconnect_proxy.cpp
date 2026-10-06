#include "proxy/backconnect_proxy.h"
#include "network/async_socket.h"
#include "network/connection_manager.h"
#include "ipv6/subnet_pool.h"
#include "ipv6/ipv6_manager.h"
#include "core/application.h"
#include "log/log_manager.h"

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>
#include <mstcpip.h>
#endif

#ifdef interface
#undef interface
#endif

namespace ProxyBridge {

BackconnectProxy::BackconnectProxy(boost::asio::io_context& ioContext,
                                   ConnectionManager& connManager,
                                   SubnetPool& pool)
    : m_ioContext(ioContext), m_connManager(connManager), m_pool(pool) {}

BackconnectProxy::~BackconnectProxy() = default;

void BackconnectProxy::connectOutbound(const std::string& targetHost, uint16_t targetPort,
    std::function<void(std::shared_ptr<AsyncSocket>, const boost::system::error_code&)> callback) {

    // Resolve the target FIRST so we know its address family.
    // IPv4 targets cannot be bound to an IPv6 source address - skip the pool
    // entirely and use the default IPv4 route (proxy stays functional for
    // IPv4-only sites; rotation applies to IPv6 traffic).
    boost::system::error_code resEc;
    boost::asio::ip::tcp::resolver resolver(m_ioContext);
    auto results = resolver.resolve(targetHost, std::to_string(targetPort), resEc);
    if (resEc) {
        Application::instance().logManager().log(LogLevel::Warning,
            "Resolve failed for " + targetHost + ": " + resEc.message(), "Backconnect");
    } else {
        bool hasV6 = false;
        for (const auto& entry : results) {
            if (!entry.endpoint().address().is_v4()) { hasV6 = true; break; }
        }
        if (!hasV6) {
            // Target is IPv4-only - cannot bind an IPv6 source; default v4 route.
            Application::instance().logManager().log(LogLevel::Info,
                "Outbound: " + targetHost + ":" + std::to_string(targetPort) +
                " | IPv4-only target, default route (no bind)", "Backconnect");
            auto remote4 = std::make_shared<AsyncSocket>(m_ioContext);
            remote4->asyncConnect(targetHost, targetPort,
                [callback, remote4](const boost::system::error_code& ec) {
                    callback(remote4, ec);
                });
            return;
        }
        // Target has IPv6 - fall through to pool binding (rotation applies here)
    }

    // Get IPv6 from pool or manager
    std::string bindAddr = m_pool.getNext();
    if (bindAddr.empty()) {
        bindAddr = Application::instance().ipv6Manager().getNextAddress();
    }

    if (!bindAddr.empty()) {
        Application::instance().logManager().log(LogLevel::Info,
            "Outbound: " + targetHost + ":" + std::to_string(targetPort) +
            " | Selected IPv6: " + bindAddr, "Backconnect");

        // Try connect with bind, with retry on mismatch
        connectWithRetry(targetHost, targetPort, bindAddr, 0, callback);
    } else {
        Application::instance().logManager().log(LogLevel::Info,
            "Outbound: " + targetHost + ":" + std::to_string(targetPort) +
            " | No IPv6 pool, default route", "Backconnect");

        auto remote = std::make_shared<AsyncSocket>(m_ioContext);
        remote->asyncConnect(targetHost, targetPort,
            [callback, remote](const boost::system::error_code& ec) {
                callback(remote, ec);
            });
    }
}

void BackconnectProxy::connectWithRetry(const std::string& targetHost, uint16_t targetPort,
    const std::string& bindAddr, int retryCount,
    std::function<void(std::shared_ptr<AsyncSocket>, const boost::system::error_code&)> callback) {

    if (retryCount >= 10) {
        Application::instance().logManager().log(LogLevel::Error,
            "BIND FAILED after 10 retries. Cannot bind to any IPv6 from pool. "
            "Rejecting connection to " + targetHost, "Backconnect");
        boost::system::error_code failEc = boost::asio::error::address_in_use;
        callback(nullptr, failEc);
        return;
    }

    auto remote = std::make_shared<AsyncSocket>(m_ioContext);

    // Use asyncConnectBind which opens socket, binds, then connects
    remote->asyncConnectBind(targetHost, targetPort, bindAddr,
        [this, remote, targetHost, targetPort, bindAddr, retryCount, callback]
        (const boost::system::error_code& ec) {
            if (ec) {
                Application::instance().logManager().log(LogLevel::Warning,
                    "Bind+Connect failed (attempt " + std::to_string(retryCount + 1) +
                    "): " + ec.message() + " | IPv6: " + bindAddr, "Backconnect");

                // Release the failed socket BEFORE creating a new one (FD leak).
                remote->close();

                // Get a different IPv6 and retry
                std::string newAddr = Application::instance().ipv6Manager().getNextAddress();
                if (newAddr.empty() || newAddr == bindAddr) {
                    Application::instance().logManager().log(LogLevel::Warning,
                        "No alternative IPv6 available. Using default route.", "Backconnect");
                    auto fallback = std::make_shared<AsyncSocket>(m_ioContext);
                    fallback->asyncConnect(targetHost, targetPort,
                        [callback, fallback](const boost::system::error_code& ec2) {
                            callback(fallback, ec2);
                        });
                    return;
                }

                Application::instance().logManager().log(LogLevel::Info,
                    "Retry " + std::to_string(retryCount + 2) + " with: " + newAddr, "Backconnect");
                connectWithRetry(targetHost, targetPort, newAddr, retryCount + 1, callback);
                return;
            }

            // SUCCESS - now verify local endpoint
            std::string localEp = remote->localEndpoint();
            Application::instance().logManager().log(LogLevel::Info,
                "Connected | Requested: " + bindAddr + " | Actual local: " + localEp, "Backconnect");

            bool verified = false;
            std::string reqNorm = bindAddr;
            if (reqNorm.size() > 2 && reqNorm.substr(reqNorm.size()-2) == "::") {
                reqNorm = reqNorm.substr(0, reqNorm.size()-2);
            }

            if (localEp.find(reqNorm) != std::string::npos) {
                verified = true;
            } else {
                std::string reqPrefix = bindAddr.substr(0, 19);
                if (localEp.find(reqPrefix) != std::string::npos) {
                    verified = true;
                }
            }

            if (!verified && !localEp.empty()) {
                Application::instance().logManager().log(LogLevel::Warning,
                    "BIND NOTE: Requested=" + bindAddr +
                    " Actual=" + localEp +
                    " | ISP overrides source address (normal for non-VPS)", "Backconnect");
            }

            if (verified) {
                Application::instance().logManager().log(LogLevel::Info,
                    "BIND VERIFIED OK: " + bindAddr, "Backconnect");
            }

            callback(remote, ec);
        });
}

// Tunnel session: owns shared_ptr refs to both sockets, runs both relay
// directions on a strand, tracks per-direction byte counts, and tears the
// pair down with a proper shutdown sequence when either side EOFs/errors or
// the idle timeout fires.
class BackconnectProxy::TunnelSession : public std::enable_shared_from_this<TunnelSession> {
public:
    TunnelSession(ConnectionManager& connManager, std::shared_ptr<AsyncSocket> client,
                  std::shared_ptr<AsyncSocket> remote)
        : m_connManager(connManager),
          m_client(std::move(client)), m_remote(std::move(remote)),
          m_strand(boost::asio::make_strand(m_connManager.ioContext())),
          m_idleTimer(m_connManager.ioContext()) {
        m_alive = true;
    }

    void start(std::shared_ptr<std::vector<uint8_t>> initialData) {
        armIdleTimer();
        if (initialData && !initialData->empty()) {
            auto self = shared_from_this();
            auto data = std::move(initialData);
            boost::asio::post(m_strand, [this, self, data] {
                m_clientToRemoteBytes += data->size();
                m_remote->asyncWrite(*data,
                    [this, self, data](const boost::system::error_code& ec, size_t) {
                        if (ec) { shutdown("initial flush failed"); return; }
                        m_connManager.recordTransfer(0, 0, data->size());
                        pump(true);
                    });
            });
        } else {
            pump(true);
        }
        pump(false);
    }

private:
    void armIdleTimer() {
        auto self = shared_from_this();
        m_idleTimer.expires_after(kIdleTimeout);
        m_idleTimer.async_wait(boost::asio::bind_executor(m_strand,
            [this, self](const boost::system::error_code& ec) {
                if (ec) return; // cancelled
                if (m_alive) {
                    Application::instance().logManager().log(LogLevel::Info,
                        "Tunnel idle timeout (" +
                        std::to_string(std::chrono::duration_cast<std::chrono::minutes>(kIdleTimeout).count()) +
                        " min) - closing", "Backconnect");
                    shutdown("idle timeout");
                }
            }));
    }

    void pump(bool clientToRemote) {
        auto self = shared_from_this();
        auto& from = clientToRemote ? m_client : m_remote;
        auto& to = clientToRemote ? m_remote : m_client;
        auto buffer = std::make_shared<std::vector<uint8_t>>(8192);

        boost::asio::post(m_strand, [this, self, clientToRemote, from, to, buffer] {
            from->asyncRead(boost::asio::buffer(*buffer),
                boost::asio::bind_executor(m_strand,
                [this, self, clientToRemote, from, to, buffer]
                (const boost::system::error_code& ec, size_t bytesRead) {
                    if (ec || bytesRead == 0) { shutdown("read EOF/error"); return; }
                    auto data = std::make_shared<std::vector<uint8_t>>(
                        buffer->begin(), buffer->begin() + static_cast<long>(bytesRead));
                    to->asyncWrite(*data,
                        boost::asio::bind_executor(m_strand,
                        [this, self, clientToRemote, from, to, data]
                        (const boost::system::error_code& wEc, size_t) {
                            if (wEc) { shutdown("write failed"); return; }
                            // Record transfer per direction.
                            if (clientToRemote) {
                                m_clientToRemoteBytes += data->size();
                                m_connManager.recordTransfer(0, data->size(), 0); // bytes from client in
                            } else {
                                m_remoteToClientBytes += data->size();
                                m_connManager.recordTransfer(0, 0, data->size()); // bytes out to client
                            }
                            m_idleTimer.expires_after(kIdleTimeout);
                            pump(clientToRemote);
                        }));
                }));
        });
    }

    void shutdown(const char* reason) {
        if (!m_alive.exchange(false)) return;
        Application::instance().logManager().log(LogLevel::Debug,
            std::string("Tunnel closed (") + reason + ") in=" +
            std::to_string(m_clientToRemoteBytes) + " out=" +
            std::to_string(m_remoteToClientBytes), "Backconnect");
        m_idleTimer.cancel();
        // Shutdown sequence: fully close both sockets.
        m_client->close();
        m_remote->close();
        // Make sure the ConnectionManager entry is released when the tunnel
        // finishes (the relay handlers no longer carry the numeric id).
        m_connManager.releaseBySocket(m_client.get());
        m_connManager.releaseBySocket(m_remote.get());
    }

    static constexpr std::chrono::minutes kIdleTimeout{10};

    ConnectionManager& m_connManager;
    std::shared_ptr<AsyncSocket> m_client;
    std::shared_ptr<AsyncSocket> m_remote;
    boost::asio::strand<boost::asio::io_context::executor_type> m_strand;
    boost::asio::steady_timer m_idleTimer;
    std::atomic<bool> m_alive{false};
    uint64_t m_clientToRemoteBytes{0};
    uint64_t m_remoteToClientBytes{0};
};

void BackconnectProxy::startTunnel(std::shared_ptr<AsyncSocket> client,
                                    std::shared_ptr<AsyncSocket> remote,
                                    std::shared_ptr<std::vector<uint8_t>> initialData) {
    auto session = std::make_shared<TunnelSession>(m_connManager, client, remote);
    session->start(std::move(initialData));
}

} // namespace ProxyBridge
