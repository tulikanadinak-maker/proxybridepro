#include "frp/frp_config.h"
#include <fstream>
#include <sstream>

namespace ProxyBridge {

std::string FrpConfigGenerator::generate(const FrpConfig& config, uint16_t localProxyPort) {
    std::ostringstream ss;

    ss << "[common]\n";
    ss << "server_addr = " << config.vpsHost << "\n";
    ss << "server_port = " << config.serverPort << "\n";
    ss << "token = " << config.token << "\n";
    ss << "protocol = " << config.protocol << "\n";
    ss << "\n";
    ss << "[[proxies]]\n";
    ss << "name = \"proxy\"\n";
    ss << "type = \"tcp\"\n";
    ss << "local_ip = \"127.0.0.1\"\n";
    ss << "local_port = " << localProxyPort << "\n";
    ss << "remote_port = " << config.proxyPort << "\n";

    if (config.dashboardPort > 0) {
        ss << "\n";
        ss << "[[proxies]]\n";
        ss << "name = \"dashboard\"\n";
        ss << "type = \"tcp\"\n";
        ss << "local_ip = \"127.0.0.1\"\n";
        ss << "local_port = 8089\n";
        ss << "remote_port = " << config.dashboardPort << "\n";
    }

    return ss.str();
}

std::string FrpConfigGenerator::generateDashboardProxy(const FrpConfig& config,
                                                        uint16_t localDashboardPort) {
    std::ostringstream ss;
    ss << "[[proxies]]\n";
    ss << "name = \"dashboard\"\n";
    ss << "type = \"tcp\"\n";
    ss << "local_ip = \"127.0.0.1\"\n";
    ss << "local_port = " << localDashboardPort << "\n";
    ss << "remote_port = " << config.dashboardPort << "\n";
    return ss.str();
}

bool FrpConfigGenerator::writeToFile(const std::string& content, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << content;
    return true;
}

} // namespace ProxyBridge
