#pragma once

#include <string>
#include <unordered_set>
#include <mutex>
#include <cstdint>
#include <atomic>

namespace ProxyBridge {

/**
 * @class AuthManager
 * @brief Password-based authentication with IP limiting
 *
 * Clients authenticate with a password. Limits the number of
 * unique IPs that can authenticate simultaneously.
 */
class AuthManager {
public:
    AuthManager();
    ~AuthManager();

    void setPassword(const std::string& password);
    std::string password() const;

    void setMaxIpCount(uint32_t count);
    uint32_t maxIpCount() const;

    void setEnabled(bool enabled);
    bool isEnabled() const;

    /**
     * @brief Check if an IP is loopback or private/LAN (RFC1918, fe80::/10)
     *
     * Shared CIDR-based check used by both AuthManager and Socks5Handler.
     */
    static bool isLanOrLocalIp(const std::string& ip);

    /**
     * @brief Authenticate a client
     * @param providedPassword Password from client
     * @param clientIp Client's IP address
     * @return true if authentication succeeded
     */
    bool authenticate(const std::string& providedPassword, const std::string& clientIp);

    /**
     * @brief Remove a client from authenticated list
     * @param clientIp Client IP to remove
     */
    void removeClient(const std::string& clientIp);

    /**
     * @brief Get count of authenticated clients
     */
    uint32_t authenticatedCount() const;

    /**
     * @brief Get all authenticated IPs
     */
    std::unordered_set<std::string> authenticatedIps() const;

    /**
     * @brief Clear all authenticated clients
     */
    void clearAll();

private:
    std::string m_password{"123456"};
    std::unordered_set<std::string> m_authenticatedIps;
    mutable std::mutex m_mutex;
    std::atomic<uint32_t> m_maxIpCount{5};
    std::atomic<bool> m_enabled{true};
};

} // namespace ProxyBridge
