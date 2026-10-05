#include "proxy/socks5_handler.h"
#include "proxy/backconnect_proxy.h"
#include "proxy/auth_manager.h"
#include "network/async_socket.h"
#include "core/application.h"
#include "log/log_manager.h"
#include <sstream>

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#endif

#ifdef interface
#undef interface
#endif

namespace ProxyBridge {

namespace {

// Compute how many bytes the SOCKS5 greeting consumed; nullopt if incomplete.
bool greetingLength(const std::vector<uint8_t>& data, size_t& len) {
    if (data.size() < 2) return false;
    len = 2u + data[1];
    return data.size() >= len;
}

struct AuthPacket {
    bool ok = false;
    std::string username;
    std::string password;
    size_t consumed = 0;
};

// RFC 1929: VER(1) ULEN(1) UNAME(1-255) PLEN(1) PASSWD(1-255)
AuthPacket parseAuthPacket(const uint8_t* data, size_t size) {
    AuthPacket pkt;
    if (size < 2 || data[0] != 0x01) return pkt;
    uint8_t uLen = data[1];
    if (size < static_cast<size_t>(2 + uLen + 1)) return pkt;
    uint8_t pLen = data[2 + uLen];
    if (size < static_cast<size_t>(2 + uLen + 1 + pLen)) return pkt;
    pkt.username.assign(reinterpret_cast<const char*>(data + 2), uLen);
    pkt.password.assign(reinterpret_cast<const char*>(data + 2 + uLen + 1), pLen);
    pkt.consumed = 2u + uLen + 1u + pLen;
    pkt.ok = true;
    return pkt;
}

} // namespace

Socks5Handler::Socks5Handler(boost::asio::io_context& ioContext,
                             BackconnectProxy& backconnect,
                             AuthManager& authManager)
    : m_ioContext(ioContext), m_backconnect(backconnect), m_authManager(authManager) {}

Socks5Handler::~Socks5Handler() = default;

void Socks5Handler::handleConnection(std::shared_ptr<AsyncSocket> client,
                                      const std::vector<uint8_t>& initialData) {
    Application::instance().logManager().log(LogLevel::Info,
        "SOCKS5 client connected from " + client->remoteEndpoint(), "SOCKS5");
    handleGreeting(client, initialData);
}

void Socks5Handler::handleGreeting(std::shared_ptr<AsyncSocket> client,
                                    const std::vector<uint8_t>& data) {
    size_t greetingLen = 0;
    if (!greetingLength(data, greetingLen)) {
        Application::instance().logManager().log(LogLevel::Error,
            "SOCKS5 greeting too short: " + std::to_string(data.size()) + " bytes", "SOCKS5");
        sendErrorReply(client, 0xFF);
        return;
    }

    if (data[0] != 0x05) {
        Application::instance().logManager().log(LogLevel::Error,
            "SOCKS5 invalid version: " + std::to_string(data[0]), "SOCKS5");
        sendErrorReply(client, 0xFF);
        return;
    }

    // Everything after the greeting is buffered as leftover for the next stage.
    auto leftover = std::make_shared<std::vector<uint8_t>>(data.begin() + greetingLen, data.end());

    uint8_t numMethods = data[1];
    bool hasNoAuth = false;
    bool hasUserPass = false;

    for (uint8_t i = 0; i < numMethods && static_cast<size_t>(2 + i) < greetingLen; ++i) {
        if (data[2 + i] == 0x00) hasNoAuth = true;
        if (data[2 + i] == 0x02) hasUserPass = true;
    }

    Application::instance().logManager().log(LogLevel::Info,
        "SOCKS5 greeting: " + std::to_string(numMethods) + " methods offered"
        " (NoAuth=" + std::string(hasNoAuth ? "yes" : "no") +
        " UserPass=" + std::string(hasUserPass ? "yes" : "no") + ")"
        " AuthRequired=" + std::string(m_authManager.isEnabled() ? "yes" : "no"), "SOCKS5");

    if (m_authManager.isEnabled()) {
        // LAN/loopback bypass using proper CIDR parsing (shared with AuthManager).
        std::string clientIp = client->remoteEndpoint();
        bool isLocal = AuthManager::isLanOrLocalIp(clientIp);

        if (isLocal && hasNoAuth) {
            auto resp = std::make_shared<std::vector<uint8_t>>(std::vector<uint8_t>{0x05, 0x00});
            client->asyncWrite(*resp, [this, client, resp, leftover](auto ec, size_t) {
                if (ec) { client->close(); return; }
                Application::instance().logManager().log(LogLevel::Info,
                    "SOCKS5 LAN bypass - NO AUTH required", "SOCKS5");
                handleRequest(client, leftover);
            });
        } else if (hasUserPass) {
            auto resp = std::make_shared<std::vector<uint8_t>>(std::vector<uint8_t>{0x05, 0x02});
            client->asyncWrite(*resp, [this, client, resp, leftover](auto ec, size_t) {
                if (ec) {
                    Application::instance().logManager().log(LogLevel::Error,
                        "SOCKS5 failed to send method selection", "SOCKS5");
                    client->close();
                    return;
                }
                Application::instance().logManager().log(LogLevel::Info,
                    "SOCKS5 method selected: USERNAME/PASSWORD (0x02)", "SOCKS5");
                handleAuth(client, leftover);
            });
        } else {
            Application::instance().logManager().log(LogLevel::Warning,
                "SOCKS5 client does not support required auth method", "SOCKS5");
            sendErrorReply(client, 0xFF);
        }
    } else {
        if (hasNoAuth) {
            auto resp = std::make_shared<std::vector<uint8_t>>(std::vector<uint8_t>{0x05, 0x00});
            client->asyncWrite(*resp, [this, client, resp, leftover](auto ec, size_t) {
                if (ec) { client->close(); return; }
                Application::instance().logManager().log(LogLevel::Info,
                    "SOCKS5 method selected: NO AUTH (0x00)", "SOCKS5");
                handleRequest(client, leftover);
            });
        } else {
            sendErrorReply(client, 0xFF);
        }
    }
}

void Socks5Handler::handleAuth(std::shared_ptr<AsyncSocket> client,
                               const std::shared_ptr<std::vector<uint8_t>>& leftover) {
    // Consume any bytes left over from the greeting stage first.
    if (!leftover->empty()) {
        auto pkt = parseAuthPacket(leftover->data(), leftover->size());
        if (!pkt.ok) {
            Application::instance().logManager().log(LogLevel::Error,
                "SOCKS5 auth packet truncated in leftover buffer", "SOCKS5");
            sendAuthReply(client, false, nullptr);
            return;
        }
        processAuth(client, pkt.password, std::make_shared<std::vector<uint8_t>>(
            leftover->begin() + pkt.consumed, leftover->end()));
        return;
    }

    auto buf = std::make_shared<std::vector<uint8_t>>(512);
    client->asyncRead(boost::asio::buffer(*buf),
        [this, client, buf](auto ec, size_t bytesRead) {
            if (ec) {
                Application::instance().logManager().log(LogLevel::Error,
                    "SOCKS5 auth read error: " + ec.message(), "SOCKS5");
                client->close();
                return;
            }

            auto pkt = parseAuthPacket(buf->data(), bytesRead);
            if (!pkt.ok) {
                Application::instance().logManager().log(LogLevel::Error,
                    "SOCKS5 auth packet invalid or truncated (" +
                    std::to_string(bytesRead) + " bytes)", "SOCKS5");
                sendAuthReply(client, false, nullptr);
                return;
            }

            auto rest = std::make_shared<std::vector<uint8_t>>(
                buf->begin() + static_cast<long>(pkt.consumed), buf->end());
            processAuth(client, pkt.password, rest);
        });
}

void Socks5Handler::processAuth(std::shared_ptr<AsyncSocket> client,
                                const std::string& password,
                                std::shared_ptr<std::vector<uint8_t>> leftover) {
    // Never log credentials (username may be sensitive, password never logged).
    Application::instance().logManager().log(LogLevel::Info,
        "SOCKS5 auth attempt for user (credentials not logged)", "SOCKS5");

    std::string clientIp = client->remoteEndpoint();
    bool authOk = m_authManager.authenticate(password, clientIp);

    Application::instance().logManager().log(
        authOk ? LogLevel::Info : LogLevel::Warning,
        "SOCKS5 auth " + std::string(authOk ? "SUCCESS" : "FAILED") +
        " for " + clientIp, "SOCKS5");

    if (!authOk) {
        sendAuthReply(client, false, nullptr);
        return;
    }
    sendAuthReply(client, true, leftover);
}

void Socks5Handler::sendAuthReply(std::shared_ptr<AsyncSocket> client, bool success,
                                  std::shared_ptr<std::vector<uint8_t>> leftover) {
    // RFC 1929: VER(1) STATUS(1) - 0x00 = success
    auto resp = std::make_shared<std::vector<uint8_t>>(
        std::vector<uint8_t>{0x01, static_cast<uint8_t>(success ? 0x00 : 0x01)});
    client->asyncWrite(*resp, [this, client, resp, success, leftover](auto ec, size_t) {
        if (ec) {
            Application::instance().logManager().log(LogLevel::Error,
                "SOCKS5 failed to send auth reply", "SOCKS5");
            client->close();
            return;
        }
        if (!success) {
            Application::instance().logManager().log(LogLevel::Info,
                "SOCKS5 closing after auth failure", "SOCKS5");
            client->close();
            return;
        }
        handleRequest(client, leftover);
    });
}

void Socks5Handler::handleRequest(std::shared_ptr<AsyncSocket> client,
                                  const std::shared_ptr<std::vector<uint8_t>>& leftover) {
    struct RequestView {
        bool ok = false;
        uint8_t ver = 0;
        uint8_t cmd = 0;
        uint8_t addrType = 0;
        std::string host;
        uint16_t port = 0;
        size_t consumed = 0;
    };

    auto parseRequest = [](const uint8_t* data, size_t size) -> RequestView {
        RequestView r;
        if (size < 4) return r;
        r.ver = data[0];
        r.cmd = data[1];
        r.addrType = data[3];
        size_t pos = 4;
        if (r.addrType == 0x01) { // IPv4
            if (size < 10) return r;
            r.host = std::to_string(data[4]) + "." + std::to_string(data[5]) + "." +
                     std::to_string(data[6]) + "." + std::to_string(data[7]);
            pos = 8;
        } else if (r.addrType == 0x03) { // Domain
            if (size < 5) return r;
            uint8_t dLen = data[4];
            if (size < static_cast<size_t>(5 + dLen + 2)) return r;
            r.host.assign(reinterpret_cast<const char*>(data + 5), dLen);
            pos = 5 + dLen;
        } else if (r.addrType == 0x04) { // IPv6
            if (size < 22) return r;
            char ipv6buf[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, data + 4, ipv6buf, sizeof(ipv6buf));
            r.host = ipv6buf;
            pos = 20;
        } else {
            return r; // unsupported address type
        }
        r.port = static_cast<uint16_t>((data[pos] << 8) | data[pos + 1]);
        r.consumed = pos + 2;
        r.ok = true;
        return r;
    };

    RequestView req;
    std::shared_ptr<std::vector<uint8_t>> newLeftover;

    if (!leftover->empty()) {
        req = parseRequest(leftover->data(), leftover->size());
        if (req.ok) {
            newLeftover = std::make_shared<std::vector<uint8_t>>(
                leftover->begin() + static_cast<long>(req.consumed), leftover->end());
        }
    }

    if (!req.ok && leftover->empty()) {
        // No buffered request: read from socket.
        auto buf = std::make_shared<std::vector<uint8_t>>(512);
        client->asyncRead(boost::asio::buffer(*buf),
            [this, client, buf](auto ec, size_t bytesRead) {
                if (ec) {
                    Application::instance().logManager().log(LogLevel::Error,
                        "SOCKS5 request read error: " + ec.message(), "SOCKS5");
                    sendErrorReply(client, 0x01);
                    return;
                }
                auto rest = std::make_shared<std::vector<uint8_t>>(buf->begin(), buf->begin() + static_cast<long>(bytesRead));
                handleRequest(client, rest);
            });
        return;
    }

    if (!req.ok) {
        // Leftover present but incomplete/invalid request in it: fall through
        // to a fresh read, prepending leftover via a combined buffer.
        auto combined = std::make_shared<std::vector<uint8_t>>(*leftover);
        auto buf = std::make_shared<std::vector<uint8_t>>(512);
        client->asyncRead(boost::asio::buffer(*buf),
            [this, client, combined, buf](auto ec, size_t bytesRead) {
                if (ec) {
                    Application::instance().logManager().log(LogLevel::Error,
                        "SOCKS5 request read error: " + ec.message(), "SOCKS5");
                    sendErrorReply(client, 0x01);
                    return;
                }
                combined->insert(combined->end(), buf->begin(), buf->begin() + static_cast<long>(bytesRead));
                handleRequest(client, combined);
            });
        return;
    }

    if (req.ver != 0x05) {
        Application::instance().logManager().log(LogLevel::Error,
            "SOCKS5 request invalid version: " + std::to_string(req.ver), "SOCKS5");
        sendErrorReply(client, 0x01);
        return;
    }

    if (req.cmd != 0x01) { // Only CONNECT supported
        Application::instance().logManager().log(LogLevel::Warning,
            "SOCKS5 unsupported command: " + std::to_string(req.cmd), "SOCKS5");
        sendErrorReply(client, 0x07);
        return;
    }

    Application::instance().logManager().log(LogLevel::Info,
        "SOCKS5 CONNECT request: " + req.host + ":" + std::to_string(req.port), "SOCKS5");

    connectTarget(client, req.host, req.port, newLeftover);
}

void Socks5Handler::connectTarget(std::shared_ptr<AsyncSocket> client,
                                   const std::string& host, uint16_t port,
                                   std::shared_ptr<std::vector<uint8_t>> leftover) {
    m_backconnect.connectOutbound(host, port,
        [this, client, host, port, leftover](std::shared_ptr<AsyncSocket> remote, const boost::system::error_code& ec) {
            if (ec) {
                Application::instance().logManager().log(LogLevel::Error,
                    "SOCKS5 outbound connect failed: " + host + ":" + std::to_string(port) +
                    " error: " + ec.message(), "SOCKS5");
                sendErrorReply(client, 0x05);
                return;
            }

            std::string bindAddr = "0.0.0.0";
            uint16_t bindPort = 0;
            try {
                boost::system::error_code epEc;
                auto ep = remote->socket().local_endpoint(epEc);
                if (!epEc) {
                    bindAddr = ep.address().to_string();
                    bindPort = ep.port();
                }
            } catch (...) {}

            Application::instance().logManager().log(LogLevel::Info,
                "SOCKS5 connected to " + host + ":" + std::to_string(port) +
                " via " + bindAddr + ":" + std::to_string(bindPort), "SOCKS5");

            // Send success reply THEN start tunnel (must be sequential)
            auto replyData = std::make_shared<std::vector<uint8_t>>();
            replyData->push_back(0x05);
            replyData->push_back(0x00); // success
            replyData->push_back(0x00); // RSV
            replyData->push_back(0x01); // ATYP IPv4

            std::istringstream iss(bindAddr);
            std::string octet;
            int cnt = 0;
            while (std::getline(iss, octet, '.') && cnt < 4) {
                try { replyData->push_back(static_cast<uint8_t>(std::stoi(octet))); }
                catch (...) { replyData->push_back(0); }
                cnt++;
            }
            while (cnt < 4) { replyData->push_back(0); cnt++; }
            replyData->push_back(static_cast<uint8_t>((bindPort >> 8) & 0xFF));
            replyData->push_back(static_cast<uint8_t>(bindPort & 0xFF));

            client->asyncWrite(*replyData,
                [this, client, remote, replyData, host, port, leftover](auto ec, size_t) {
                    if (ec) {
                        Application::instance().logManager().log(LogLevel::Error,
                            "SOCKS5 failed to send success reply", "SOCKS5");
                        client->close();
                        remote->close();
                        return;
                    }
                    Application::instance().logManager().log(LogLevel::Info,
                        "SOCKS5 relay started for " + host + ":" + std::to_string(port), "SOCKS5");
                    m_backconnect.startTunnel(client, remote, leftover);
                });
        });
}

void Socks5Handler::sendMethodSelection(std::shared_ptr<AsyncSocket> client, uint8_t method) {
    // Not used anymore - inline in handleGreeting
    auto resp = std::make_shared<std::vector<uint8_t>>(std::vector<uint8_t>{0x05, method});
    client->asyncWrite(*resp, [client, resp](auto, size_t) {});
}

void Socks5Handler::sendReply(std::shared_ptr<AsyncSocket> client, uint8_t reply,
                               const std::string& bindAddr, uint16_t bindPort) {
    auto resp = std::make_shared<std::vector<uint8_t>>();
    resp->push_back(0x05);
    resp->push_back(reply);
    resp->push_back(0x00); // RSV
    resp->push_back(0x01); // ATYP = IPv4

    std::istringstream iss(bindAddr);
    std::string octet;
    int cnt = 0;
    while (std::getline(iss, octet, '.') && cnt < 4) {
        try { resp->push_back(static_cast<uint8_t>(std::stoi(octet))); }
        catch (...) { resp->push_back(0); }
        cnt++;
    }
    while (cnt < 4) { resp->push_back(0); cnt++; }

    resp->push_back(static_cast<uint8_t>((bindPort >> 8) & 0xFF));
    resp->push_back(static_cast<uint8_t>(bindPort & 0xFF));

    client->asyncWrite(*resp, [client, resp, reply](auto, size_t) {
        client->close();
    });
}

void Socks5Handler::sendErrorReply(std::shared_ptr<AsyncSocket> client, uint8_t reply) {
    if (reply == 0xFF) {
        // Greeting stage: method-selection reply signalling "no acceptable
        // methods", then close.
        auto resp = std::make_shared<std::vector<uint8_t>>(std::vector<uint8_t>{0x05, 0xFF});
        client->asyncWrite(*resp, [client, resp](auto, size_t) { client->close(); });
        return;
    }
    // Request stage: general SOCKS5 reply (5, REP, RSV, ATYP=IPv4, 0.0.0.0:0),
    // then close. Never close silently.
    sendReply(client, reply, "0.0.0.0", 0);
}

} // namespace ProxyBridge
