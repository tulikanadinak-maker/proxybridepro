#include "proxy/http_handler.h"
#include "proxy/backconnect_proxy.h"
#include "proxy/auth_manager.h"
#include "network/async_socket.h"
#include "core/application.h"
#include "log/log_manager.h"
#include <cctype>
#include <sstream>

namespace ProxyBridge {

namespace {

// Decode base64 (standard alphabet, '=' padding, whitespace tolerated).
std::string decodeBase64(const std::string& in) {
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::string out;
    out.reserve((in.size() / 4) * 3);
    int acc = 0, bits = 0;
    for (char c : in) {
        if (c == '=') break;
        int v = val(c);
        if (v < 0) continue; // skip whitespace / CR / LF
        acc = (acc << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((acc >> bits) & 0xFF));
        }
    }
    return out;
}

bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) return false;
    return true;
}

// Extract just the IP from an endpoint string ("1.2.3.4:5678" or "[v6]:port").
std::string endpointToIp(const std::string& endpoint) {
    if (!endpoint.empty() && endpoint.front() == '[') {
        auto close = endpoint.find(']');
        if (close != std::string::npos) return endpoint.substr(1, close - 1);
    }
    auto colon = endpoint.rfind(':');
    if (colon != std::string::npos && endpoint.find(':') == colon) {
        return endpoint.substr(0, colon); // single colon -> IPv4:port
    }
    return endpoint; // bare IPv6 or empty
}

} // namespace

HttpHandler::HttpHandler(boost::asio::io_context& ioContext,
                         BackconnectProxy& backconnect,
                         AuthManager& authManager)
    : m_ioContext(ioContext), m_backconnect(backconnect), m_authManager(authManager) {}

HttpHandler::~HttpHandler() = default;

void HttpHandler::handleConnection(std::shared_ptr<AsyncSocket> client,
                                    const std::vector<uint8_t>& initialData) {
    std::string raw(initialData.begin(), initialData.end());
    HttpRequest req = parseRequest(raw);
    if (req.method.empty()) { sendError(client, 400, "Bad Request"); return; }

    Application::instance().logManager().log(LogLevel::Info,
        "HTTP " + req.method + " " + req.host + ":" + std::to_string(req.port) +
        " from " + client->remoteEndpoint(), "HTTP");

    if (!authenticate(req, client)) return;

    if (req.isConnect) handleConnect(client, req);
    else handleRelay(client, req, raw);
}

HttpRequest HttpHandler::parseRequest(const std::string& raw) {
    HttpRequest req;
    std::istringstream stream(raw);
    std::string line;
    if (!std::getline(stream, line)) return req;
    if (!line.empty() && line.back() == '\r') line.pop_back();

    std::istringstream reqLine(line);
    reqLine >> req.method >> req.url >> req.version;

    // Split host[:port], handling IPv6 literals "[2001:db8::1]:443".
    auto splitHostPort = [](const std::string& hp, uint16_t defaultPort,
                            std::string& host, uint16_t& port) {
        port = defaultPort;
        if (!hp.empty() && hp.front() == '[') {
            auto close = hp.find(']');
            if (close != std::string::npos) {
                host = hp.substr(1, close - 1);
                if (close + 2 <= hp.size() && hp[close + 1] == ':') {
                    try { port = static_cast<uint16_t>(std::stoi(hp.substr(close + 2))); }
                    catch (...) { port = defaultPort; }
                }
                return;
            }
        }
        auto colon = hp.rfind(':');
        if (colon != std::string::npos && hp.find(':') == colon) {
            // Exactly one colon: IPv4 or hostname with port.
            host = hp.substr(0, colon);
            try { port = static_cast<uint16_t>(std::stoi(hp.substr(colon + 1))); }
            catch (...) { port = defaultPort; }
        } else {
            host = hp; // bare IPv6 or hostname without port
        }
    };

    if (req.method == "CONNECT") {
        req.isConnect = true;
        splitHostPort(req.url, 443, req.host, req.port);
    } else {
        std::string url = req.url;
        uint16_t defaultPort = 80;
        if (url.rfind("http://", 0) == 0) url = url.substr(7);
        else if (url.rfind("https://", 0) == 0) { url = url.substr(8); defaultPort = 443; }
        auto slash = url.find('/');
        std::string hp = (slash != std::string::npos) ? url.substr(0, slash) : url;
        splitHostPort(hp, defaultPort, req.host, req.port);
    }

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        auto pos = line.find(':');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string val = line.substr(pos + 1);
            val.erase(0, val.find_first_not_of(" \t"));
            // Header names are case-insensitive (RFC 7230); normalize to lower.
            for (auto& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            req.headers[key] = val;
        }
    }

    if (req.host.empty()) {
        auto it = req.headers.find("host");
        if (it != req.headers.end()) {
            std::string host;
            uint16_t port = 0;
            splitHostPort(it->second, req.port, host, port);
            req.host = host;
            if (port != 0) req.port = port;
        }
    }

    return req;
}

bool HttpHandler::authenticate(const HttpRequest& request, std::shared_ptr<AsyncSocket> client) {
    if (!m_authManager.isEnabled()) return true;

    // Header lookup is case-insensitive (keys stored lowercased).
    auto it = request.headers.find("proxy-authorization");
    if (it == request.headers.end()) {
        std::string resp = "HTTP/1.1 407 Proxy Authentication Required\r\n"
                           "Proxy-Authenticate: Basic realm=\"ProxyBridge\"\r\n"
                           "Content-Length: 0\r\n\r\n";
        client->asyncWrite(resp, [client](auto, size_t) { client->close(); });
        return false;
    }

    // Parse "Basic base64(user:password)".
    const std::string& auth = it->second;
    const std::string basic = "Basic ";
    std::string credentials;
    if (auth.size() > basic.size() && iequals(auth.substr(0, basic.size()), basic)) {
        credentials = decodeBase64(auth.substr(basic.size()));
    }

    if (!credentials.empty()) {
        auto colonPos = credentials.find(':');
        std::string password = (colonPos != std::string::npos)
            ? credentials.substr(colonPos + 1) : std::string();

        std::string clientIp = endpointToIp(client->remoteEndpoint());
        if (m_authManager.authenticate(password, clientIp)) return true;
    }

    sendError(client, 403, "Forbidden");
    return false;
}

void HttpHandler::handleConnect(std::shared_ptr<AsyncSocket> client, const HttpRequest& req) {
    m_backconnect.connectOutbound(req.host, req.port,
        [this, client, req](std::shared_ptr<AsyncSocket> remote, const boost::system::error_code& ec) {
            if (ec) { sendError(client, 502, "Bad Gateway"); return; }
            std::string resp = req.version + " 200 Connection Established\r\n"
                               "Proxy-Agent: ProxyBridgePro/2.0\r\n\r\n";
            client->asyncWrite(resp, [this, client, remote](auto ec, size_t) {
                if (ec) { client->close(); remote->close(); return; }
                m_backconnect.startTunnel(client, remote);
            });
        });
}

void HttpHandler::handleRelay(std::shared_ptr<AsyncSocket> client, const HttpRequest& req,
                               const std::string& rawRequest) {
    m_backconnect.connectOutbound(req.host, req.port,
        [this, client, rawRequest](std::shared_ptr<AsyncSocket> remote, const boost::system::error_code& ec) {
            if (ec) { sendError(client, 502, "Bad Gateway"); return; }
            remote->asyncWrite(rawRequest, [this, client, remote](auto ec, size_t) {
                if (ec) { sendError(client, 502, "Bad Gateway"); return; }
                m_backconnect.startTunnel(client, remote);
            });
        });
}

void HttpHandler::sendError(std::shared_ptr<AsyncSocket> client, int code, const std::string& msg) {
    std::string resp = "HTTP/1.1 " + std::to_string(code) + " " + msg + "\r\n"
                       "Content-Length: 0\r\nConnection: close\r\n\r\n";
    client->asyncWrite(resp, [client](auto, size_t) { client->close(); });
}

} // namespace ProxyBridge
