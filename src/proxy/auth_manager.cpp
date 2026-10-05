#include "proxy/auth_manager.h"
#include <array>
#include <cctype>

namespace ProxyBridge {

namespace {

// Parse a decimal octet string (no sign, no leading '+'); returns false on
// malformed input. Rejects leading whitespace etc. via strict digit loop.
bool parseOctet(const std::string& s, unsigned& out) {
    if (s.empty() || s.size() > 3) return false;
    unsigned v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        v = v * 10 + static_cast<unsigned>(c - '0');
    }
    if (v > 255) return false;
    out = v;
    return true;
}

// Strict dotted-quad IPv4 parse -> 32-bit host-order value.
bool parseIpv4(const std::string& ip, uint32_t& out) {
    std::array<unsigned, 4> o{};
    size_t start = 0;
    for (int i = 0; i < 4; ++i) {
        size_t dot = ip.find('.', start);
        std::string part;
        if (i < 3) {
            if (dot == std::string::npos) return false;
            part = ip.substr(start, dot - start);
            start = dot + 1;
        } else {
            if (dot != std::string::npos) return false; // no 5th octet
            part = ip.substr(start);
        }
        if (!parseOctet(part, o[i])) return false;
    }
    out = (static_cast<uint32_t>(o[0]) << 24) |
          (static_cast<uint32_t>(o[1]) << 16) |
          (static_cast<uint32_t>(o[2]) << 8) |
           static_cast<uint32_t>(o[3]);
    return true;
}

// Check whether ipv4 is inside network/prefixLen.
bool inCidr(uint32_t ipv4, uint32_t network, unsigned prefixLen) {
    if (prefixLen > 32) return false;
    uint32_t mask = (prefixLen == 0) ? 0u : (0xFFFFFFFFu << (32 - prefixLen));
    return (ipv4 & mask) == (network & mask);
}

// Strip an optional ":port" suffix and brackets from an endpoint string.
std::string stripPort(const std::string& endpoint) {
    if (!endpoint.empty() && endpoint.front() == '[') {
        // bracketed IPv6: [addr]:port
        auto close = endpoint.find(']');
        if (close != std::string::npos) return endpoint.substr(1, close - 1);
    }
    // Bare address or addr:port (v4 / unbracketed v6). Only strip a trailing
    // :port when the remainder still looks like an IPv4 literal.
    auto colon = endpoint.rfind(':');
    if (colon != std::string::npos && endpoint.find(':') == colon) {
        return endpoint.substr(0, colon);
    }
    return endpoint;
}

} // namespace

AuthManager::AuthManager() = default;
AuthManager::~AuthManager() = default;

void AuthManager::setPassword(const std::string& password) { m_password = password; }
std::string AuthManager::password() const { return m_password; }

void AuthManager::setMaxIpCount(uint32_t count) { m_maxIpCount = count; }
uint32_t AuthManager::maxIpCount() const { return m_maxIpCount; }

void AuthManager::setEnabled(bool enabled) { m_enabled = enabled; }
bool AuthManager::isEnabled() const { return m_enabled; }

bool AuthManager::isLanOrLocalIp(const std::string& ipRaw) {
    std::string ip = stripPort(ipRaw);
    if (ip.empty()) return false;

    // Normalize case for IPv6 comparisons.
    std::string lower;
    lower.reserve(ip.size());
    for (char c : ip) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

    // IPv6 loopback and link-local fe80::/10
    if (lower == "::1" || lower == "::ffff:127.0.0.1" || lower == "localhost") return true;
    if (lower == "::" ) return true;
    // fe80::/10 -> first 10 bits == 1111111010; leading hextet in [0xfe80,0xfebf]
    if (lower.rfind("fe80", 0) == 0 || lower.rfind("fe8", 0) == 0 ||
        lower.rfind("fe9", 0) == 0 || lower.rfind("fea", 0) == 0 ||
        lower.rfind("feb", 0) == 0) {
        return true;
    }

    uint32_t v4 = 0;
    if (!parseIpv4(lower, v4)) return false;

    // RFC1918 private ranges + loopback
    if (inCidr(v4, 0x7F000000u, 8))  return true; // 127.0.0.0/8
    if (inCidr(v4, 0x0A000000u, 8))  return true; // 10.0.0.0/8
    if (inCidr(v4, 0xAC100000u, 12)) return true; // 172.16.0.0/12
    if (inCidr(v4, 0xC0A80000u, 16)) return true; // 192.168.0.0/16

    return false;
}

bool AuthManager::authenticate(const std::string& providedPassword, const std::string& clientIp) {
    if (!m_enabled) return true;

    // Allow localhost and LAN clients without password. These are NOT added
    // to m_authenticatedIps (they are not authenticated identities).
    if (isLanOrLocalIp(clientIp)) {
        return true;
    }

    if (providedPassword != m_password) return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_authenticatedIps.count(clientIp)) return true;
    if (m_authenticatedIps.size() >= m_maxIpCount) return false;

    m_authenticatedIps.insert(clientIp);
    return true;
}

void AuthManager::removeClient(const std::string& clientIp) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_authenticatedIps.erase(clientIp);
}

uint32_t AuthManager::authenticatedCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<uint32_t>(m_authenticatedIps.size());
}

std::unordered_set<std::string> AuthManager::authenticatedIps() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_authenticatedIps;
}

void AuthManager::clearAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_authenticatedIps.clear();
}

} // namespace ProxyBridge
