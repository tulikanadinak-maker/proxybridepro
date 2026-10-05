#include "frp/frp_client.h"
#include "frp/frp_config.h"
#include <filesystem>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace ProxyBridge {

FrpClient::FrpClient() = default;

FrpClient::~FrpClient() { stop(); }

bool FrpClient::start(const FrpConfig& config) {
    if (m_running) return true;
    m_config = config;
    if (!m_config.enabled) return false;

    m_running = true;
    m_status = FrpStatus::Connecting;
    if (m_statusCallback) m_statusCallback(m_status, "Connecting to " + m_config.vpsHost);

    generateConfigFile();
    m_processThread = std::thread([this] { runProcess(); });

    return true;
}

void FrpClient::stop() {
    m_running = false;

#ifdef _WIN32
    if (m_processHandle) {
        TerminateProcess(m_processHandle, 0);
        CloseHandle(m_processHandle);
        m_processHandle = nullptr;
    }
#endif

    if (m_processThread.joinable()) m_processThread.join();

    m_status = FrpStatus::Disconnected;
    if (m_statusCallback) m_statusCallback(m_status, "Disconnected");
}

bool FrpClient::isConnected() const { return m_status == FrpStatus::Connected; }
FrpStatus FrpClient::status() const { return m_status; }

void FrpClient::setConfig(const FrpConfig& config) { m_config = config; }
const FrpConfig& FrpClient::config() const { return m_config; }
void FrpClient::setStatusCallback(FrpStatusCallback callback) { m_statusCallback = std::move(callback); }

std::string FrpClient::connectionInfo() const {
    std::string info;
    info += "FRP C2S local → public access via VPS\n";
    info += "FRP C2T: local proxy only\n";
    info += "FRP mode format: " + modeString() + "\n";
    info += "Local mode format: 127.0.0.1:" + std::to_string(m_config.proxyPort) + "\n";
    info += "Dashboard: http://127.0.0.1:" + std::to_string(m_config.dashboardPort) +
            "/dashboard?token=" + m_config.token + "\n";
    info += "Rotate url: http://127.0.0.1:" + std::to_string(m_config.dashboardPort) +
            "/rotate?user=1&token=" + m_config.token + "\n";
    return info;
}

std::string FrpClient::modeString() const {
    return m_config.vpsHost + ":" + std::to_string(m_config.proxyPort) +
           ":user:" + m_config.token;
}

void FrpClient::generateConfigFile() {
    // Write frpc config to temp directory
#ifdef _WIN32
    char* tmpDir = nullptr; size_t len = 0;
    _dupenv_s(&tmpDir, &len, "TEMP");
    std::string dir = tmpDir ? std::string(tmpDir) : ".";
    if (tmpDir) free(tmpDir);
    m_configPath = dir + "\\proxybridge_frpc.toml";
#else
    const char* tmpDir = std::getenv("TMPDIR");
    std::string dir = (tmpDir && *tmpDir) ? std::string(tmpDir) : "/tmp";
    m_configPath = dir + "/proxybridge_frpc.toml";
#endif

    std::string content = FrpConfigGenerator::generate(m_config, m_config.proxyPort);
    FrpConfigGenerator::writeToFile(content, m_configPath);
}

void FrpClient::runProcess() {
#ifdef _WIN32
    // Look for frpc.exe in app directory or PATH
    std::string exePath = "frpc.exe";
    std::string cmdLine = exePath + " -c \"" + m_configPath + "\"";

    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (CreateProcessA(nullptr, const_cast<char*>(cmdLine.c_str()),
                       nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                       nullptr, nullptr, &si, &pi)) {
        m_processHandle = pi.hProcess;
        CloseHandle(pi.hThread);

        // Wait a moment then assume connected
        Sleep(3000);
        if (m_running) {
            m_status = FrpStatus::Connected;
            if (m_statusCallback) m_statusCallback(m_status, "Connected to " + m_config.vpsHost);
        }

        // Wait for process to exit
        WaitForSingleObject(pi.hProcess, INFINITE);
        m_processHandle = nullptr;
        CloseHandle(pi.hProcess);

        if (m_running) {
            m_status = FrpStatus::Error;
            if (m_statusCallback) m_statusCallback(m_status, "FRP process exited");
        }
    } else {
        m_status = FrpStatus::Error;
        if (m_statusCallback) m_statusCallback(m_status, "Failed to start frpc.exe");
    }
#endif
    m_running = false;
}

} // namespace ProxyBridge
