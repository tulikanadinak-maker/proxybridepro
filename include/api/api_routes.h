#pragma once

#include <string>

namespace ProxyBridge {

class ApiServer;
class Application;

/**
 * @brief Register all API routes
 *
 * Endpoints:
 * GET  /dashboard?token=xxx         - Get status/stats
 * GET  /rotate?user=1&token=xxx     - Rotate IPs
 * GET  /reset?token=xxx             - Reset IPs
 * GET  /status?token=xxx            - Server status
 * GET  /slots?token=xxx             - Active slots info
 * POST /config?token=xxx            - Update config
 */
void registerApiRoutes(ApiServer& server);

} // namespace ProxyBridge
