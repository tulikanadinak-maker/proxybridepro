#pragma once

/**
 * @file frp_client.h
 * @brief FRP (Fast Reverse Proxy) client integration
 *
 * Exposes the local proxy to the internet via a VPS running frps.
 * Manages the frpc subprocess and monitors connection status.
 */

#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <cstdint>
#include <functional>

namespace ProxyBridge {

struct FrpConfig {
    bool enabled = false;
    std::string vpsHost = "127.0.0.1";
    uint16_t serverPort = 7000;
    std::string token = "12345678";
    uint16_t proxyPort = 1080;       // Remote port for proxy
    uint16_t dashboardPort = 8089;   // Remote dashboard port
    std::string protocol = "tcp";     // tcp or kcp
};

enum class FrpStatus : uint8_t {
    Disconnected,
    Connecting,
    Connected,
    Error
};

using FrpStatusCallback = std::function<void(FrpStatus, const std::string&)>;

class FrpClient {
public:
    FrpClient();
    ~FrpClient();

    bool start(const FrpConfig& config);
    void stop();
    bool isConnected() const;
    FrpStatus status() const;

    void setConfig(const FrpConfig& config);
    const FrpConfig& config() const;

    void setStatusCallback(FrpStatusCallback callback);

    /**
     * @brief Get connection info string
     */
    std::string connectionInfo() const;

    /**
     * @brief Get FRP mode format string for display
     */
    std::string modeString() const;

private:
    void runProcess();
    void generateConfigFile();
    void monitorProcess();

    FrpConfig m_config;
    FrpStatusCallback m_statusCallback;
    std::atomic<FrpStatus> m_status{FrpStatus::Disconnected};
    std::atomic<bool> m_running{false};
    std::thread m_processThread;
    std::mutex m_mutex;
    void* m_processHandle{nullptr}; // HANDLE on Windows
    std::string m_configPath;
};

} // namespace ProxyBridge
