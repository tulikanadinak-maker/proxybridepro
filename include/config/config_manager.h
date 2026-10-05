#pragma once

#include <string>
#include <filesystem>
#include <mutex>
#include <nlohmann/json.hpp>
#include "proxy/proxy_server.h"
#include "frp/frp_client.h"
#include "api/api_server.h"
#include "ipv6/ipv6_rotator.h"

namespace ProxyBridge {

struct AppConfig {
    // Proxy
    ProxyConfig proxy;

    // FRP
    FrpConfig frp;

    // API
    ApiConfig api;

    // Rotation
    RotationMode rotationMode = RotationMode::Manual;
    uint32_t rotationInterval = 300;
    uint32_t rotationRequestCount = 100;

    // Transport
    bool customUdpProtocol = true;
    bool killSwitch = false;
    uint32_t canaryPercent = 100;

    // General
    bool darkMode = true;
    bool autoStart = false;
    bool startMinimized = false;
    std::string logLevel = "info";
};

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();

    bool initialize();
    bool load();
    bool save();

    AppConfig config() const;  // returns a copy under the lock (thread-safe snapshot)
    void updateConfig(const AppConfig& config);

    std::filesystem::path configDir() const;
    bool backup(const std::filesystem::path& path);
    bool restore(const std::filesystem::path& path);
    void resetToDefaults();

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);

private:
    void createDefaultConfig();
    std::filesystem::path configFilePath() const;

    AppConfig m_config;
    std::filesystem::path m_configDir;
    mutable std::mutex m_mutex;
    bool m_initialized{false};
    bool m_weakSecretReplaced{false};  // set by fromJson when weak secrets regenerated
public:
    bool apiTokenRegenerated() const { return m_weakSecretReplaced; }
private:
};

} // namespace ProxyBridge
