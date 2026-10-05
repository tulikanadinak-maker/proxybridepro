#pragma once

#include <string>
#include "frp/frp_client.h"

namespace ProxyBridge {

/**
 * @brief Generates frpc.toml configuration file content
 */
class FrpConfigGenerator {
public:
    static std::string generate(const FrpConfig& config, uint16_t localProxyPort);
    static std::string generateDashboardProxy(const FrpConfig& config, uint16_t localDashboardPort);
    static bool writeToFile(const std::string& content, const std::string& path);
};

} // namespace ProxyBridge
