#pragma once

#include <memory>
#include <atomic>
#include <string>

namespace ProxyBridge {

class ThreadPool;
class EventBus;
class ConfigManager;
class LogManager;
class IPv6Manager;
class ProxyServer;
class FrpClient;
class ApiServer;
class SubnetPool;
class SourceManager;

/**
 * @class Application
 * @brief Singleton application controller for IPv6 Rotating Proxy
 */
class Application {
public:
    static Application& instance();

    bool initialize();
    void shutdown();
    bool isRunning() const;
    std::string version() const;

    ThreadPool& threadPool();
    EventBus& eventBus();
    ConfigManager& configManager();
    LogManager& logManager();
    IPv6Manager& ipv6Manager();
    ProxyServer& proxyServer();
    FrpClient& frpClient();
    ApiServer& apiServer();

    // Control
    bool startProxy();
    void stopProxy();
    void rotateIp();
    void resetIp();

private:
    Application();
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    std::unique_ptr<ThreadPool> m_threadPool;
    std::unique_ptr<EventBus> m_eventBus;
    std::unique_ptr<ConfigManager> m_configManager;
    std::unique_ptr<LogManager> m_logManager;
    std::unique_ptr<IPv6Manager> m_ipv6Manager;
    std::unique_ptr<ProxyServer> m_proxyServer;
    std::unique_ptr<FrpClient> m_frpClient;
    std::unique_ptr<ApiServer> m_apiServer;
    std::unique_ptr<SourceManager> m_sourceManager;
    std::unique_ptr<SubnetPool> m_subnetPool;
    std::atomic<bool> m_running{false};
};

} // namespace ProxyBridge
