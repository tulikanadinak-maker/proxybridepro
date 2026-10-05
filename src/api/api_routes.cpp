#include "api/api_routes.h"
#include "api/api_server.h"
#include "core/application.h"
#include "config/config_manager.h"
#include "ipv6/ipv6_manager.h"
#include "proxy/proxy_server.h"
#include "network/connection_manager.h"
#include <nlohmann/json.hpp>

namespace ProxyBridge {

void registerApiRoutes(ApiServer& server) {

    // GET /dashboard - Status dashboard
    server.registerRoute("GET", "/dashboard", [](const std::string&) -> std::string {
        auto& app = Application::instance();
        auto& stats = app.proxyServer().connectionManager().stats();

        nlohmann::json j;
        j["status"] = app.proxyServer().isRunning() ? "running" : "stopped";
        j["version"] = app.version();
        j["connections"] = stats.activeConnections.load();
        j["totalConnections"] = stats.totalConnections.load();
        j["bytesIn"] = stats.totalBytesIn.load();
        j["bytesOut"] = stats.totalBytesOut.load();
        j["successRequests"] = stats.successfulRequests.load();
        j["failedRequests"] = stats.failedRequests.load();
        j["slots"] = app.ipv6Manager().slotCount();
        j["activeSlots"] = app.ipv6Manager().getActiveSlots().size();
        return j.dump();
    });

    // GET /rotate - Rotate all IPs
    server.registerRoute("GET", "/rotate", [](const std::string&) -> std::string {
        Application::instance().rotateIp();
        nlohmann::json j;
        j["status"] = "ok";
        j["message"] = "IP rotation triggered";
        return j.dump();
    });

    // GET /reset - Reset IPs
    server.registerRoute("GET", "/reset", [](const std::string&) -> std::string {
        Application::instance().resetIp();
        nlohmann::json j;
        j["status"] = "ok";
        j["message"] = "IP reset triggered";
        return j.dump();
    });

    // GET /status - Server status
    server.registerRoute("GET", "/status", [](const std::string&) -> std::string {
        auto& app = Application::instance();
        nlohmann::json j;
        j["running"] = app.proxyServer().isRunning();
        j["port"] = app.proxyServer().port();
        j["ipv6Active"] = app.ipv6Manager().isActive();
        j["slots"] = app.ipv6Manager().slotCount();
        return j.dump();
    });

    // GET /slots - Get active slot information
    server.registerRoute("GET", "/slots", [](const std::string&) -> std::string {
        auto& app = Application::instance();
        auto slots = app.ipv6Manager().getActiveSlots();

        nlohmann::json arr = nlohmann::json::array();
        for (const auto& slot : slots) {
            nlohmann::json s;
            s["id"] = slot.id;
            s["address"] = slot.address.toString();
            s["active"] = slot.active;
            s["requests"] = slot.requestCount;
            s["bytes"] = slot.bytesTransferred;
            arr.push_back(s);
        }

        nlohmann::json j;
        j["slots"] = arr;
        j["total"] = slots.size();
        return j.dump();
    });
}

} // namespace ProxyBridge
