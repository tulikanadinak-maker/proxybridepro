#include "gui/frp_page.h"
#include "core/application.h"
#include "config/config_manager.h"
#include <QGroupBox>
#include <QFormLayout>
#include <QFrame>

namespace ProxyBridge {

FrpPage::FrpPage(QWidget* parent) : QWidget(parent) { setupUi(); loadConfig(); }
FrpPage::~FrpPage() = default;

void FrpPage::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 32, 32, 32); layout->setSpacing(16);

    auto* title = new QLabel("FRP Tunnel");
    title->setObjectName("title"); layout->addWidget(title);
    auto* subtitle = new QLabel("Expose your local proxy to the internet via FRP reverse tunnel.");
    subtitle->setObjectName("subtitle"); layout->addWidget(subtitle);

    m_enableCheck = new QCheckBox("Enable FRP Tunnel");
    m_enableCheck->setStyleSheet("font-size: 14px;");
    layout->addWidget(m_enableCheck);

    layout->addWidget(createServerSection());
    layout->addWidget(createPortsSection());
    layout->addWidget(createInfoSection());
    layout->addStretch();
}

QWidget* FrpPage::createServerSection() {
    auto* group = new QGroupBox("Server"); auto* form = new QFormLayout(group);
    m_vpsHostEdit = new QLineEdit("127.0.0.1"); form->addRow("VPS HOST", m_vpsHostEdit);
    m_serverPortSpin = new QSpinBox(); m_serverPortSpin->setRange(1, 65535); m_serverPortSpin->setValue(7000);
    m_tokenEdit = new QLineEdit("12345678");
    form->addRow("SERVER PORT", m_serverPortSpin); form->addRow("TOKEN", m_tokenEdit);
    return group;
}

QWidget* FrpPage::createPortsSection() {
    auto* group = new QGroupBox("Public Ports"); auto* form = new QFormLayout(group);
    m_proxyPortSpin = new QSpinBox(); m_proxyPortSpin->setRange(1, 65535); m_proxyPortSpin->setValue(1080);
    m_dashboardPortSpin = new QSpinBox(); m_dashboardPortSpin->setRange(1, 65535); m_dashboardPortSpin->setValue(8089);
    form->addRow("PROXY PORT", m_proxyPortSpin); form->addRow("DASHBOARD PORT", m_dashboardPortSpin);
    return group;
}

QWidget* FrpPage::createInfoSection() {
    auto* frame = new QFrame(); frame->setObjectName("card");
    frame->setStyleSheet("background-color: #1e3a5f; border: 1px solid #3b82f6; border-radius: 8px; padding: 16px;");
    auto* layout = new QVBoxLayout(frame);
    auto* infoTitle = new QLabel("\xe2\x84\xb9 Connection Info");
    infoTitle->setStyleSheet("color: #60a5fa; font-weight: 600;");
    layout->addWidget(infoTitle);
    m_connectionInfoLabel = new QLabel("FRP C2S local → public access via VPS\nStart FRP to see connection details.");
    m_connectionInfoLabel->setWordWrap(true);
    m_connectionInfoLabel->setStyleSheet("color: #93c5fd; font-family: 'Consolas', monospace; font-size: 12px;");
    layout->addWidget(m_connectionInfoLabel);
    return frame;
}

void FrpPage::loadConfig() {
    auto cfg = Application::instance().configManager().config();
    m_enableCheck->setChecked(cfg.frp.enabled);
    m_vpsHostEdit->setText(QString::fromStdString(cfg.frp.vpsHost));
    m_serverPortSpin->setValue(cfg.frp.serverPort);
    m_tokenEdit->setText(QString::fromStdString(cfg.frp.token));
    m_proxyPortSpin->setValue(cfg.frp.proxyPort);
    m_dashboardPortSpin->setValue(cfg.frp.dashboardPort);
}

void FrpPage::saveConfig() {
    auto cfg = Application::instance().configManager().config();
    cfg.frp.enabled = m_enableCheck->isChecked();
    cfg.frp.vpsHost = m_vpsHostEdit->text().toStdString();
    cfg.frp.serverPort = static_cast<uint16_t>(m_serverPortSpin->value());
    cfg.frp.token = m_tokenEdit->text().toStdString();
    cfg.frp.proxyPort = static_cast<uint16_t>(m_proxyPortSpin->value());
    cfg.frp.dashboardPort = static_cast<uint16_t>(m_dashboardPortSpin->value());
    Application::instance().configManager().updateConfig(cfg);
    Application::instance().configManager().save();
}

void FrpPage::updateConnectionInfo(const QString& info) {
    m_connectionInfoLabel->setText(info);
}

} // namespace ProxyBridge
