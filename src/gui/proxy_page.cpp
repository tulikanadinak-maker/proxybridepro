#include "gui/proxy_page.h"
#include "core/application.h"
#include "config/config_manager.h"
#include "log/log_manager.h"
#include "proxy/proxy_server.h"
#include "network/connection_manager.h"
#include <QGroupBox>
#include <QFormLayout>
#include <QTimer>

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>
#endif

#ifdef interface
#undef interface
#endif

namespace ProxyBridge {

ProxyPage::ProxyPage(QWidget* parent) : QWidget(parent) {
    setupUi();
    loadConfig();
}

ProxyPage::~ProxyPage() = default;

void ProxyPage::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    auto* title = new QLabel("Proxy Settings");
    title->setObjectName("title");
    layout->addWidget(title);
    auto* subtitle = new QLabel("Configure proxy behavior, authentication, and network binding.");
    subtitle->setObjectName("subtitle");
    layout->addWidget(subtitle);

    layout->addWidget(createNetworkSection());
    layout->addWidget(createAuthSection());
    layout->addWidget(createBindingSection());
    layout->addWidget(createProtocolSection());

    // Buttons
    auto* btnLayout = new QHBoxLayout();
    m_rotateButton = new QPushButton("Rotate IP");
    m_rotateButton->setMinimumHeight(40);
    m_resetButton = new QPushButton("Reset IP");
    m_resetButton->setMinimumHeight(40);
    m_resetButton->setStyleSheet("background-color: #7f1d1d; color: white; border: 1px solid #ef4444;");
    connect(m_rotateButton, &QPushButton::clicked, this, &ProxyPage::onRotateIp);
    connect(m_resetButton, &QPushButton::clicked, this, &ProxyPage::onResetIp);
    btnLayout->addWidget(m_rotateButton);
    btnLayout->addWidget(m_resetButton);
    layout->addLayout(btnLayout);

    layout->addStretch();
}

QWidget* ProxyPage::createNetworkSection() {
    auto* group = new QGroupBox("Network");
    auto* form = new QFormLayout(group);
    m_modeCombo = new QComboBox();
    m_modeCombo->addItems({"WiFi / ISP", "Ethernet", "Custom"});
    form->addRow("MODE", m_modeCombo);
    m_multiSourceCheck = new QCheckBox("Enable Multi Source");
    form->addRow(m_multiSourceCheck);
    return group;
}

QWidget* ProxyPage::createAuthSection() {
    auto* group = new QGroupBox("Authentication");
    auto* form = new QFormLayout(group);
    m_passwordEdit = new QLineEdit();
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    form->addRow("PASSWORD", m_passwordEdit);
    m_ipCountSpin = new QSpinBox();
    m_ipCountSpin->setRange(1, 100);
    m_ipCountSpin->setValue(5);
    form->addRow("IP COUNT", m_ipCountSpin);
    return group;
}

QWidget* ProxyPage::createBindingSection() {
    auto* group = new QGroupBox("Local Binding");
    auto* form = new QFormLayout(group);

    // Host as dropdown with auto-detected options
    m_hostEdit = new QLineEdit();
    // We'll use a QComboBox instead
    auto* hostCombo = new QComboBox();
    hostCombo->setEditable(true);
    hostCombo->addItem("0.0.0.0 (All interfaces - recommended)");
    hostCombo->addItem("127.0.0.1 (Localhost only)");

    // Detect local IPs
#ifdef _WIN32
    ULONG bufLen = 15000;
    auto* addresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(malloc(bufLen));
    if (addresses) {
        DWORD result = GetAdaptersAddresses(AF_INET, 0, nullptr, addresses, &bufLen);
        if (result == ERROR_BUFFER_OVERFLOW) {
            free(addresses);
            addresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(malloc(bufLen));
            if (addresses)
                result = GetAdaptersAddresses(AF_INET, 0, nullptr, addresses, &bufLen);
        }
        if (result == NO_ERROR && addresses) {
            for (auto* adapter = addresses; adapter; adapter = adapter->Next) {
                if (adapter->OperStatus != IfOperStatusUp) continue;
                for (auto* unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
                    auto* sa = reinterpret_cast<sockaddr_in*>(unicast->Address.lpSockaddr);
                    if (sa->sin_family == AF_INET) {
                        char ip[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
                        std::string ipStr(ip);
                        if (ipStr != "127.0.0.1") {
                            char name[256];
                            WideCharToMultiByte(CP_UTF8, 0, adapter->FriendlyName, -1, name, sizeof(name), nullptr, nullptr);
                            hostCombo->addItem(QString::fromStdString(ipStr + " (" + name + ")"));
                        }
                    }
                }
            }
        }
        if (addresses) free(addresses);
    }
#endif

    hostCombo->setCurrentIndex(0);
    form->addRow("HOST", hostCombo);

    // Store combo for later
    m_hostEdit->setVisible(false);
    // Replace m_hostEdit with combo functionally
    connect(hostCombo, &QComboBox::currentTextChanged, [this](const QString& text) {
        // Extract just the IP part (before the space)
        QString ip = text.split(" ").first();
        m_hostEdit->setText(ip);
    });
    m_hostEdit->setText("0.0.0.0");

    m_portSpin = new QSpinBox();
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(1080);
    form->addRow("PORT", m_portSpin);

    return group;
}

QWidget* ProxyPage::createProtocolSection() {
    auto* group = new QGroupBox("Protocols");
    auto* form = new QFormLayout(group);
    m_socks5Check = new QCheckBox("SOCKS5 (recommended for VPN clients / apps)");
    m_httpCheck = new QCheckBox("HTTP/HTTPS (recommended for browsers)");
    m_socks5Check->setChecked(true);
    m_httpCheck->setChecked(true);
    form->addRow(m_socks5Check);
    form->addRow(m_httpCheck);
    return group;
}

void ProxyPage::loadConfig() {
    auto cfg = Application::instance().configManager().config();
    m_passwordEdit->setText(QString::fromStdString(cfg.proxy.authPassword));
    m_ipCountSpin->setValue(static_cast<int>(cfg.proxy.maxIpCount));
    m_hostEdit->setText(QString::fromStdString(cfg.proxy.bindHost));
    m_portSpin->setValue(cfg.proxy.bindPort);
    m_socks5Check->setChecked(cfg.proxy.enableSocks5);
    m_httpCheck->setChecked(cfg.proxy.enableHttp);
}

void ProxyPage::saveConfig() {
    auto cfg = Application::instance().configManager().config();
    cfg.proxy.authPassword = m_passwordEdit->text().toStdString();
    cfg.proxy.maxIpCount = static_cast<uint32_t>(m_ipCountSpin->value());
    cfg.proxy.bindHost = m_hostEdit->text().toStdString();
    cfg.proxy.bindPort = static_cast<uint16_t>(m_portSpin->value());
    cfg.proxy.enableSocks5 = m_socks5Check->isChecked();
    cfg.proxy.enableHttp = m_httpCheck->isChecked();
    Application::instance().configManager().updateConfig(cfg);
    Application::instance().configManager().save();
}

void ProxyPage::onRotateIp() { Application::instance().rotateIp(); }
void ProxyPage::onResetIp() { Application::instance().resetIp(); }

} // namespace ProxyBridge
