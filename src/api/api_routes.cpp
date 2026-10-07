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
    server.registerRoute("GET", "/dashboard", [](const std::string&, const std::string&) -> std::string {
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
    server.registerRoute("GET", "/rotate", [](const std::string&, const std::string&) -> std::string {
        Application::instance().rotateIp();
        nlohmann::json j;
        j["status"] = "ok";
        j["message"] = "IP rotation triggered";
        return j.dump();
    });

    // GET /reset - Reset IPs
    server.registerRoute("GET", "/reset", [](const std::string&, const std::string&) -> std::string {
        Application::instance().resetIp();
        nlohmann::json j;
        j["status"] = "ok";
        j["message"] = "IP reset triggered";
        return j.dump();
    });

    // GET /status - Server status
    server.registerRoute("GET", "/status", [](const std::string&, const std::string&) -> std::string {
        auto& app = Application::instance();
        nlohmann::json j;
        j["running"] = app.proxyServer().isRunning();
        j["port"] = app.proxyServer().port();
        j["ipv6Active"] = app.ipv6Manager().isActive();
        j["slots"] = app.ipv6Manager().slotCount();
        return j.dump();
    });

    // GET /slots - Get active slot information
    server.registerRoute("GET", "/slots", [](const std::string&, const std::string&) -> std::string {
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

    // GET /panel - HTML dashboard with per-slot "Change IP" buttons.
    // Mirrors the YbridgeV2 control panel: each client gets slot=<id> links;
    // /panel?slot=N rotates only that slot, plain /panel lists everything.
    server.registerRoute("GET", "/panel", [](const std::string&, const std::string&) -> std::string {
        auto& app = Application::instance();
        auto slots = app.ipv6Manager().getActiveSlots();

        std::string rows;
        for (const auto& slot : slots) {
            rows += "<tr><td>User " + std::to_string(slot.id + 1) + "</td>" +
                    "<td id='ip-" + std::to_string(slot.id) + "'>" +
                    (slot.active ? slot.address.toString() : "(inactive)") + "</td>" +
                    "<td>" + std::to_string(slot.requestCount) + "</td>" +
                    "<td><a href='/panel?slot=" + std::to_string(slot.id) + "' " +
                    "onclick='return changeIp(event," + std::to_string(slot.id) + ")'>Change IP</a></td></tr>";
        }

        return std::string("<!DOCTYPE html><html><head><meta charset='utf-8'>"
            "<title>Witeck Proxy Unlimited</title><style>"
            "body{background:#0d1117;color:#e6edf3;font-family:sans-serif;margin:2rem}"
            "table{border-collapse:collapse;width:100%}"
            "th,td{border:1px solid #30363d;padding:.6rem;text-align:left}"
            "th{background:#161b22}"
            "a{color:#10b981;text-decoration:none}"
            "h1{color:#10b981}"
            "</style>"
            "<script>async function changeIp(ev,user){ev.preventDefault();"
            "const ip=document.getElementById('ip-'+user);"
            "ip.textContent='rotating...';"
            "const r=await fetch('/rotate-slot?slot='+user);"
            "const j=await r.json();"
            "ip.textContent=j.address||('error: '+(j.error||'unknown'));"
            "return false;}</script>"
            "</head><body><h1>Witeck Proxy Unlimited</h1>"
            "<p>Slots: " + std::to_string(slots.size()) + "</p>"
            "<table><tr><th>User</th><th>IPv6 egress</th><th>Requests</th><th>Action</th></tr>" +
            rows + "</table></body></html>");
    });

    // GET /rotate-slot?slot=N - rotate exactly one slot, return new address.
    server.registerRoute("GET", "/rotate-slot", [](const std::string&, const std::string& query) -> std::string {
        uint32_t slotId = UINT32_MAX;
        size_t p = query.find("slot=");
        if (p != std::string::npos) {
            try { slotId = static_cast<uint32_t>(std::stoul(query.substr(p + 5))); }
            catch (...) { return "{\"error\":\"bad slot\"}"; }
        }
        if (slotId == UINT32_MAX) return "{\"error\":\"missing slot\"}";
        auto& app = Application::instance();
        if (!app.ipv6Manager().rotateSlot(slotId))
            return "{\"error\":\"slot not found or bind failed\"}";
        for (const auto& s : app.ipv6Manager().getActiveSlots()) {
            if (s.id == slotId)
                return "{\"status\":\"ok\",\"slot\":" + std::to_string(slotId) +
                       ",\"address\":\"" + s.address.toString() + "\"}";
        }
        return "{\"error\":\"unknown\"}";
    });
}

} // namespace ProxyBridge
