#include "core/application.h"
#include "core/thread_pool.h"
#include "core/event_bus.h"
#include "config/config_manager.h"
#include "log/log_manager.h"
#include "ipv6/ipv6_manager.h"
#include "ipv6/subnet_pool.h"
#include "ipv6/source_manager.h"
#include "proxy/proxy_server.h"
#include "frp/frp_client.h"
#include "api/api_server.h"
#include "api/api_routes.h"

// Kill Windows COM macro
#ifdef interface
#undef interface
#endif

namespace ProxyBridge {

Application& Application::instance() {
    static Application app;
    return app;
}

Application::Application() = default;
Application::~Application() { shutdown(); }

bool Application::initialize() {
    if (m_running) return true;

    m_threadPool = std::make_unique<ThreadPool>(0);
    m_eventBus = std::make_unique<EventBus>();

    m_configManager = std::make_unique<ConfigManager>();
    if (!m_configManager->initialize()) return false;

    m_logManager = std::make_unique<LogManager>();
    if (!m_logManager->initialize()) return false;

    m_ipv6Manager = std::make_unique<IPv6Manager>();

    // Create source manager and subnet pool
    m_sourceManager = std::make_unique<SourceManager>();
    m_subnetPool = std::make_unique<SubnetPool>(*m_sourceManager);

    const auto& cfg = m_configManager->config();
    m_proxyServer = std::make_unique<ProxyServer>(cfg.proxy);
    m_frpClient = std::make_unique<FrpClient>();
    m_apiServer = std::make_unique<ApiServer>(cfg.api);

    // Register API routes
    registerApiRoutes(*m_apiServer);

    m_running = true;
    m_logManager->log(LogLevel::Info, "ProxyBridge Pro v2.0 initialized", "Application");
    return true;
}

void Application::shutdown() {
    if (!m_running) return;
    m_running = false;

    if (m_logManager) m_logManager->log(LogLevel::Info, "Shutting down...", "Application");

    if (m_frpClient) m_frpClient->stop();
    if (m_apiServer) m_apiServer->stop();
    if (m_proxyServer) m_proxyServer->stop();

    m_apiServer.reset();
    m_frpClient.reset();
    m_proxyServer.reset();
    m_ipv6Manager.reset();
    m_logManager.reset();
    m_configManager.reset();
    m_eventBus.reset();
    if (m_threadPool) { m_threadPool->stop(); m_threadPool.reset(); }
}

bool Application::isRunning() const { return m_running; }
std::string Application::version() const { return "2.0.0"; }

ThreadPool& Application::threadPool() { return *m_threadPool; }
EventBus& Application::eventBus() { return *m_eventBus; }
ConfigManager& Application::configManager() { return *m_configManager; }
LogManager& Application::logManager() { return *m_logManager; }
IPv6Manager& Application::ipv6Manager() { return *m_ipv6Manager; }
ProxyServer& Application::proxyServer() { return *m_proxyServer; }
FrpClient& Application::frpClient() { return *m_frpClient; }
ApiServer& Application::apiServer() { return *m_apiServer; }

bool Application::startProxy() {
    // Reload config from persistent storage
    const auto cfg = m_configManager->config();

    // Log what we're actually using
    m_logManager->log(LogLevel::Info, "=== Starting Proxy ===", "Application");
    m_logManager->log(LogLevel::Info, "  Host: " + cfg.proxy.bindHost, "Application");
    m_logManager->log(LogLevel::Info, "  Port: " + std::to_string(cfg.proxy.bindPort), "Application");
    m_logManager->log(LogLevel::Info, "  Auth: " + std::string(cfg.proxy.requireAuth ? "enabled" : "disabled"), "Application");
    m_logManager->log(LogLevel::Info, "  Max IP Count: " + std::to_string(cfg.proxy.maxIpCount), "Application");

    // BUG 1 FIX: Update ProxyServer config from current settings before start
    m_proxyServer->setConfig(cfg.proxy);

    // BUG 2 FIX: Start IPv6 if configured
    if (m_ipv6Manager->slotCount() > 0 && !m_ipv6Manager->isActive()) {
        m_logManager->log(LogLevel::Info, "  IPv6Manager: starting with " +
            std::to_string(m_ipv6Manager->slotCount()) + " slots...", "Application");
        bool ipv6ok = m_ipv6Manager->start();
        if (ipv6ok) {
            auto slots = m_ipv6Manager->getActiveSlots();
            m_logManager->log(LogLevel::Info, "  IPv6: " + std::to_string(slots.size()) +
                " addresses bound successfully", "Application");
        } else {
            m_logManager->log(LogLevel::Warning,
                "  IPv6: Failed to bind addresses (run as Admin?)", "Application");
        }
    } else if (m_ipv6Manager->isActive()) {
        auto slots = m_ipv6Manager->getActiveSlots();
        m_logManager->log(LogLevel::Info, "  IPv6: pool active with " +
            std::to_string(slots.size()) + " addresses", "Application");
    } else {
        m_logManager->log(LogLevel::Info, "  IPv6: no pool configured (using default route)", "Application");
    }

    // Start the proxy server (bind pool addresses to the interface first)
    m_subnetPool->build();
    m_subnetPool->bindAll();
    bool ok = m_proxyServer->start(*m_subnetPool);
    if (!ok) {
        m_logManager->log(LogLevel::Error, "Failed to start proxy server!", "Application");
        return false;
    }

    m_logManager->log(LogLevel::Info,
        "Proxy server listening on " + cfg.proxy.bindHost + ":" +
        std::to_string(cfg.proxy.bindPort), "Application");

    m_eventBus->publish(Event(EventType::ProxyStarted));
    return true;
}

void Application::stopProxy() {
    if (m_proxyServer->isRunning()) {
        m_proxyServer->stop();
        m_logManager->log(LogLevel::Info, "Proxy server stopped", "Application");
    }

    // Priority 3: Cleanup - unbind all IPv6 addresses
    if (m_ipv6Manager->isActive()) {
        m_ipv6Manager->stop();
        m_logManager->log(LogLevel::Info, "IPv6 pool cleaned up - all addresses unbound", "Application");
    }

    m_eventBus->publish(Event(EventType::ProxyStopped));
}

void Application::rotateIp() {
    m_ipv6Manager->rotateAll();
    m_logManager->log(LogLevel::Info, "IP rotation triggered", "Application");
    m_eventBus->publish(Event(EventType::IpRotated));
}

void Application::resetIp() {
    m_ipv6Manager->resetAll();
    m_logManager->log(LogLevel::Info, "IP reset triggered", "Application");
    m_eventBus->publish(Event(EventType::IpReset));
}

} // namespace ProxyBridge
