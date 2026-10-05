#include "api/api_server.h"
#include "network/async_socket.h"
#include <sstream>

namespace ProxyBridge {

ApiServer::ApiServer(const ApiConfig& config) : m_config(config) {}
ApiServer::~ApiServer() { stop(); }

bool ApiServer::start() {
    if (m_running || !m_config.enabled) return false;

    try {
        auto ep = boost::asio::ip::tcp::endpoint(
            boost::asio::ip::make_address(m_config.bindHost), m_config.port);
        m_acceptor = std::make_unique<boost::asio::ip::tcp::acceptor>(m_ioContext);
        m_acceptor->open(ep.protocol());
        m_acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        m_acceptor->bind(ep);
        m_acceptor->listen(128);

        m_running = true;
        startAccept();

        m_threads.emplace_back([this] { m_ioContext.run(); });
        m_threads.emplace_back([this] { m_ioContext.run(); });

        return true;
    } catch (...) { return false; }
}

void ApiServer::stop() {
    if (!m_running) return;
    m_running = false;
    if (m_acceptor) { boost::system::error_code ec; m_acceptor->close(ec); }
    m_ioContext.stop();
    for (auto& t : m_threads) if (t.joinable()) t.join();
    m_threads.clear();
    m_ioContext.restart();
}

bool ApiServer::isRunning() const { return m_running; }
void ApiServer::setConfig(const ApiConfig& config) { m_config = config; }
const ApiConfig& ApiServer::config() const { return m_config; }

void ApiServer::registerRoute(const std::string& method, const std::string& path, ApiHandler handler) {
    m_routes[method + " " + path] = std::move(handler);
}

void ApiServer::startAccept() {
    if (!m_running) return;
    auto socket = std::make_shared<AsyncSocket>(m_ioContext);
    m_acceptor->async_accept(socket->socket(),
        [this, socket](auto ec) {
            if (!ec) handleConnection(socket);
            if (m_running) startAccept();
        });
}

void ApiServer::handleConnection(std::shared_ptr<AsyncSocket> client) {
    // Accumulate the request until the "\r\n\r\n" header terminator is seen -
    // a single fixed-size read can split headers across TCP segments.
    // readMore() re-arms a 30s steady_timer on every read so a slowloris
    // client that dribbles bytes across connections can never hold the
    // connection open indefinitely.
    auto buffer = std::make_shared<std::string>();
    auto timer = std::make_shared<boost::asio::steady_timer>(m_ioContext);
    readMore(client, buffer, timer);
}

void ApiServer::readMore(std::shared_ptr<AsyncSocket> client,
                         std::shared_ptr<std::string> buffer,
                         std::shared_ptr<boost::asio::steady_timer> timer) {
    constexpr size_t kMaxHeaderBytes = 16 * 1024;

    // (Re)arm the deadline for this read/write round-trip.
    timer->expires_after(std::chrono::seconds(30));
    timer->async_wait([client, timer](const boost::system::error_code& ec) {
        if (!ec) client->close();  // timed out - drop the slow/stalled client
    });

    auto chunk = std::make_shared<std::vector<uint8_t>>(4096);
    client->asyncRead(boost::asio::buffer(*chunk),
        [this, client, buffer, timer, chunk](auto ec, size_t bytesRead) {
            boost::system::error_code ignore;
            timer->cancel(ignore);
            if (ec) { client->close(); return; }
            buffer->append(reinterpret_cast<const char*>(chunk->data()), bytesRead);

            if (buffer->find("\r\n\r\n") == std::string::npos) {
                if (buffer->size() > kMaxHeaderBytes) {
                    sendResponse(client, 431, "application/json",
                                 "{\"error\":\"headers too large\"}");
                    return;
                }
                readMore(client, buffer, timer);
                return;
            }
            processRequest(client, *buffer);
        });
}

void ApiServer::processRequest(std::shared_ptr<AsyncSocket> client, const std::string& raw) {
    // Parse HTTP request line
    std::istringstream stream(raw);
    std::string method, path, version;
    stream >> method >> path >> version;

    // Validate token
    if (!validateToken(raw)) {
        sendResponse(client, 401, "application/json", "{\"error\":\"unauthorized\"}");
        return;
    }

    // Strip query params for route matching
    std::string routePath = path;
    auto qpos = routePath.find('?');
    if (qpos != std::string::npos) routePath = routePath.substr(0, qpos);

    // Find handler
    std::string key = method + " " + routePath;
    auto it = m_routes.find(key);
    if (it != m_routes.end()) {
        std::string body;
        auto bodyStart = raw.find("\r\n\r\n");
        if (bodyStart != std::string::npos) body = raw.substr(bodyStart + 4);
        std::string result = it->second(body);
        sendResponse(client, 200, "application/json", result);
    } else {
        sendResponse(client, 404, "application/json", "{\"error\":\"not found\"}");
    }
}

void ApiServer::sendResponse(std::shared_ptr<AsyncSocket> client, int code,
                              const std::string& contentType, const std::string& body) {
    std::string status;
    switch (code) {
        case 200: status = "OK"; break;
        case 401: status = "Unauthorized"; break;
        case 404: status = "Not Found"; break;
        case 431: status = "Request Header Fields Too Large"; break;
        default: status = "Error"; break;
    }

    std::ostringstream resp;
    resp << "HTTP/1.1 " << code << " " << status << "\r\n";
    resp << "Content-Type: " << contentType << "\r\n";
    resp << "Content-Length: " << body.size() << "\r\n";
    resp << "Access-Control-Allow-Origin: *\r\n";
    resp << "Connection: close\r\n\r\n";
    resp << body;

    client->asyncWrite(resp.str(), [client](auto, size_t) { client->close(); });
}

bool ApiServer::validateToken(const std::string& rawRequest) {
    // Extract ONLY the request line (first line) and its path - never scan
    // the whole raw request (a header or body could smuggle "token=...").
    auto lineEnd = rawRequest.find("\r\n");
    std::string requestLine = rawRequest.substr(0,
        lineEnd == std::string::npos ? rawRequest.size() : lineEnd);

    auto spacePos = requestLine.find(' ');
    if (spacePos == std::string::npos) return false;
    auto pathEnd = requestLine.find(' ', spacePos + 1);
    std::string path = requestLine.substr(spacePos + 1,
        pathEnd == std::string::npos ? std::string::npos : pathEnd - spacePos - 1);

    auto qpos = path.find('?');
    if (qpos == std::string::npos) return false;
    std::string query = path.substr(qpos + 1);

    // Find token=<value> in the query string only. Never log the token.
    std::string token;
    size_t start = 0;
    bool found = false;
    while (start <= query.size()) {
        auto amp = query.find('&', start);
        std::string pair = query.substr(start,
            amp == std::string::npos ? std::string::npos : amp - start);
        if (pair.rfind("token=", 0) == 0) {
            token = pair.substr(6);
            found = true;
            break;
        }
        if (amp == std::string::npos) break;
        start = amp + 1;
    }
    if (!found) return false;

    // Constant-time comparison - XOR-accumulate differences so timing does
    // not leak how many leading characters matched.
    const std::string& expected = m_config.token;
    volatile uint8_t diff = 0;
    if (token.size() != expected.size()) {
        for (size_t i = 0; i < expected.size(); ++i) {
            diff |= static_cast<uint8_t>(expected[i]) ^
                    static_cast<uint8_t>(token.size() > i ? token[i] : 0);
        }
        return false;
    }
    for (size_t i = 0; i < expected.size(); ++i) {
        diff |= static_cast<uint8_t>(expected[i]) ^ static_cast<uint8_t>(token[i]);
    }
    return diff == 0;
}

} // namespace ProxyBridge
