#include "config/config_manager.h"
#include <fstream>
#include <random>
#include <array>

namespace {

// Random alphanumeric string - used instead of hardcoded weak defaults.
std::string generateRandomSecret(size_t length) {
    static const char charset[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static thread_local std::mt19937_64 rng([] {
        std::random_device rd;
        std::seed_seq seed{rd(), rd(), rd(), rd()};
        return std::mt19937_64(seed);
    }());
    std::uniform_int_distribution<size_t> dist(0, sizeof(charset) - 2);
    std::string out;
    out.reserve(length);
    for (size_t i = 0; i < length; ++i) out += charset[dist(rng)];
    return out;
}

} // anonymous namespace

namespace ProxyBridge {

ConfigManager::ConfigManager() = default;
ConfigManager::~ConfigManager() = default;

bool ConfigManager::initialize() {
#ifdef _WIN32
    char* appData = nullptr; size_t len = 0;
    if (_dupenv_s(&appData, &len, "APPDATA") == 0 && appData) {
        m_configDir = std::filesystem::path(appData) / "ProxyBridgePro";
        free(appData);
    } else {
        m_configDir = std::filesystem::current_path() / "config";
    }
#else
    {
        const char* xdg = std::getenv("XDG_CONFIG_HOME");
        const char* home = std::getenv("HOME");
        std::filesystem::path base;
        if (xdg && *xdg) base = xdg;
        else if (home && *home) base = std::filesystem::path(home) / ".config";
        else base = std::filesystem::current_path();
        m_configDir = base / "ProxyBridgePro";
    }
#endif

    std::error_code ec;
    std::filesystem::create_directories(m_configDir, ec);
    if (ec) return false;

    if (std::filesystem::exists(configFilePath())) {
        if (!load()) createDefaultConfig();
    } else {
        createDefaultConfig(); save();
    }

    m_initialized = true;
    return true;
}

bool ConfigManager::load() {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        std::ifstream file(configFilePath());
        if (!file.is_open()) return false;
        nlohmann::json j; file >> j;
        fromJson(j);
        if (m_weakSecretReplaced) {
            // A weak/missing secret was replaced with a random one - persist
            // so the owner can actually use the generated credentials.
            m_weakSecretReplaced = false;
            m_mutex.unlock();
            save();
            m_mutex.lock();
        }
        return true;
    } catch (...) { return false; }
}

bool ConfigManager::save() {
    // Serialize under the lock, then write to a temp file and rename -
    // an interrupted write can never leave a half-written config.
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        j = toJson();
    }
    try {
        std::filesystem::create_directories(m_configDir);
        auto finalPath = configFilePath();
        auto tmpPath = finalPath;
        tmpPath += ".tmp";
        {
            std::ofstream file(tmpPath, std::ios::binary | std::ios::trunc);
            if (!file.is_open()) return false;
            file << j.dump(4);
            file.flush();
            if (!file.good()) return false;
        }
        std::filesystem::rename(tmpPath, finalPath);
        return true;
    } catch (...) { return false; }
}

AppConfig ConfigManager::config() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;  // copy taken under the lock - no torn reads
}
void ConfigManager::updateConfig(const AppConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex); m_config = config;
}

std::filesystem::path ConfigManager::configDir() const { return m_configDir; }

bool ConfigManager::backup(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::copy_file(configFilePath(), path,
        std::filesystem::copy_options::overwrite_existing, ec);
    return !ec;
}

bool ConfigManager::restore(const std::filesystem::path& path) {
    try {
        std::ifstream file(path); if (!file.is_open()) return false;
        nlohmann::json j; file >> j;
        { std::lock_guard<std::mutex> lock(m_mutex); fromJson(j); }
        return save();
    } catch (...) { return false; }
}

void ConfigManager::resetToDefaults() {
    std::lock_guard<std::mutex> lock(m_mutex); m_config = AppConfig{};
}

nlohmann::json ConfigManager::toJson() const {
    nlohmann::json j;
    // Proxy
    j["proxy"]["bindHost"] = m_config.proxy.bindHost;
    j["proxy"]["bindPort"] = m_config.proxy.bindPort;
    j["proxy"]["maxConnections"] = m_config.proxy.maxConnections;
    j["proxy"]["connectionTimeout"] = m_config.proxy.connectionTimeout;
    j["proxy"]["enableHttp"] = m_config.proxy.enableHttp;
    j["proxy"]["enableSocks5"] = m_config.proxy.enableSocks5;
    j["proxy"]["requireAuth"] = m_config.proxy.requireAuth;
    j["proxy"]["authPassword"] = m_config.proxy.authPassword;
    j["proxy"]["maxIpCount"] = m_config.proxy.maxIpCount;
    // FRP
    j["frp"]["enabled"] = m_config.frp.enabled;
    j["frp"]["vpsHost"] = m_config.frp.vpsHost;
    j["frp"]["serverPort"] = m_config.frp.serverPort;
    j["frp"]["token"] = m_config.frp.token;
    j["frp"]["proxyPort"] = m_config.frp.proxyPort;
    j["frp"]["dashboardPort"] = m_config.frp.dashboardPort;
    // API
    j["api"]["bindHost"] = m_config.api.bindHost;
    j["api"]["port"] = m_config.api.port;
    j["api"]["token"] = m_config.api.token;
    j["api"]["enabled"] = m_config.api.enabled;
    // Rotation
    j["rotation"]["mode"] = static_cast<int>(m_config.rotationMode);
    j["rotation"]["interval"] = m_config.rotationInterval;
    j["rotation"]["requestCount"] = m_config.rotationRequestCount;
    // Transport
    j["transport"]["customUdp"] = m_config.customUdpProtocol;
    j["transport"]["killSwitch"] = m_config.killSwitch;
    j["transport"]["canaryPercent"] = m_config.canaryPercent;
    // General
    j["general"]["darkMode"] = m_config.darkMode;
    j["general"]["autoStart"] = m_config.autoStart;
    j["general"]["logLevel"] = m_config.logLevel;
    return j;
}

void ConfigManager::fromJson(const nlohmann::json& j) {
    if (j.contains("proxy")) {
        auto& p = j["proxy"];
        m_config.proxy.bindHost = p.value("bindHost", "0.0.0.0");
        m_config.proxy.bindPort = p.value("bindPort", uint16_t(1080));
        m_config.proxy.maxConnections = p.value("maxConnections", uint32_t(10000));
        m_config.proxy.connectionTimeout = p.value("connectionTimeout", uint32_t(30));
        m_config.proxy.enableHttp = p.value("enableHttp", true);
        m_config.proxy.enableSocks5 = p.value("enableSocks5", true);
        m_config.proxy.requireAuth = p.value("requireAuth", true);
        m_config.proxy.authPassword = p.value("authPassword", generateRandomSecret(16));
        if (m_config.proxy.authPassword.empty() || m_config.proxy.authPassword == "123456") {
            m_config.proxy.authPassword = generateRandomSecret(16);
            m_weakSecretReplaced = true;
        }
        m_config.proxy.maxIpCount = p.value("maxIpCount", uint32_t(5));
    }
    if (j.contains("frp")) {
        auto& f = j["frp"];
        m_config.frp.enabled = f.value("enabled", false);
        m_config.frp.vpsHost = f.value("vpsHost", "127.0.0.1");
        m_config.frp.serverPort = f.value("serverPort", uint16_t(7000));
        m_config.frp.token = f.value("token", generateRandomSecret(16));
        if (m_config.frp.token.empty() || m_config.frp.token == "12345678") {
            m_config.frp.token = generateRandomSecret(16);
        }
        m_config.frp.proxyPort = f.value("proxyPort", uint16_t(1080));
        m_config.frp.dashboardPort = f.value("dashboardPort", uint16_t(8089));
    }
    if (j.contains("api")) {
        auto& a = j["api"];
        m_config.api.bindHost = a.value("bindHost", "127.0.0.1");
        m_config.api.port = a.value("port", uint16_t(8089));
        m_config.api.token = a.value("token", generateRandomSecret(24));
        if (m_config.api.token.empty() || m_config.api.token == "123456") {
            m_config.api.token = generateRandomSecret(24);
            m_weakSecretReplaced = true;  // persist after load
        }
        m_config.api.enabled = a.value("enabled", true);
    }
    if (j.contains("rotation")) {
        auto& r = j["rotation"];
        m_config.rotationMode = RotationMode::Manual;
        {
            int modeVal = r.value("mode", 0);
            if (modeVal >= 0 && modeVal <= 3) {
                m_config.rotationMode = static_cast<RotationMode>(modeVal);
            }
        }
        m_config.rotationInterval = r.value("interval", uint32_t(300));
        m_config.rotationRequestCount = r.value("requestCount", uint32_t(100));
    }
    if (j.contains("transport")) {
        auto& t = j["transport"];
        m_config.customUdpProtocol = t.value("customUdp", true);
        m_config.killSwitch = t.value("killSwitch", false);
        m_config.canaryPercent = t.value("canaryPercent", uint32_t(100));
    }
    if (j.contains("general")) {
        auto& g = j["general"];
        m_config.darkMode = g.value("darkMode", true);
        m_config.autoStart = g.value("autoStart", false);
        m_config.logLevel = g.value("logLevel", "info");
    }
}

void ConfigManager::createDefaultConfig() {
    m_config = AppConfig{};
    // Generate random secrets instead of shipping hardcoded weak defaults
    // ("123456" etc.). The header defaults are non-empty, so treat those
    // known-weak values as "unset" too.
    auto weak = [](const std::string& s) {
        return s.empty() || s == "123456" || s == "12345678";
    };
    if (weak(m_config.proxy.authPassword)) {
        m_config.proxy.authPassword = generateRandomSecret(16);
    }
    if (weak(m_config.api.token)) {
        m_config.api.token = generateRandomSecret(24);
    }
    if (weak(m_config.frp.token)) {
        m_config.frp.token = generateRandomSecret(16);
    }
}
std::filesystem::path ConfigManager::configFilePath() const { return m_configDir / "config.json"; }

} // namespace ProxyBridge
