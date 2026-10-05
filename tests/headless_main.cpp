// Headless test harness for ProxyBridgePro core (Linux)
#include <csignal>
#include <iostream>
#include <thread>
#include <chrono>
#include "core/application.h"
#include "config/config_manager.h"
#include "proxy/proxy_server.h"

using namespace ProxyBridge;

int main(int argc, char* argv[]) {
    std::signal(SIGINT, [](int){ Application::instance().shutdown(); std::_Exit(0); });

    auto& core = Application::instance();
    if (!core.initialize()) { std::cerr << "init failed\n"; return 1; }

    // Configure: bind 1080, auth on with known password; API on 8089
    auto cfg = core.configManager().config();
    cfg.proxy.bindHost = "127.0.0.1";
    cfg.proxy.bindPort = 1080;
    cfg.proxy.requireAuth = false;   // test without auth first
    cfg.proxy.enableSocks5 = true;
    cfg.proxy.enableHttp = true;
    cfg.api.enabled = true;
    cfg.api.port = 8089;
    core.configManager().updateConfig(cfg);

    bool ok = core.startProxy();
    std::cout << "startProxy: " << (ok ? "OK" : "FAILED") << "\n";
    std::cout << "proxy listening on 127.0.0.1:" << core.proxyServer().port() << "\n";
    core.apiServer().start();
    std::cout << "api started: " << (core.apiServer().isRunning() ? "OK" : "FAILED") << "\n";

    // Run for N seconds or until SIGINT
    int seconds = (argc > 1) ? std::atoi(argv[1]) : 300;
    for (int i = 0; i < seconds && core.isRunning(); ++i)
        std::this_thread::sleep_for(std::chrono::seconds(1));

    core.shutdown();
    std::cout << "clean exit\n";
    return 0;
}
