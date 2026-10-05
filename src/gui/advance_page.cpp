#include "gui/advance_page.h"
#include "core/application.h"
#include "config/config_manager.h"
#include <QGroupBox>
#include <QFormLayout>
#include <QFrame>

namespace ProxyBridge {

AdvancePage::AdvancePage(QWidget* parent) : QWidget(parent) { setupUi(); loadConfig(); }
AdvancePage::~AdvancePage() = default;

void AdvancePage::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 32, 32, 32); layout->setSpacing(16);

    auto* title = new QLabel("Advanced");
    title->setObjectName("title"); layout->addWidget(title);
    auto* subtitle = new QLabel("Experimental transport and failover settings.");
    subtitle->setObjectName("subtitle"); layout->addWidget(subtitle);

    layout->addWidget(createTransportSection());
    layout->addWidget(createRolloutSection());

    // Experimental warning
    auto* warn = new QFrame(); warn->setObjectName("card");
    warn->setStyleSheet("background-color: #3d2e00; border: 1px solid #f59e0b; border-radius: 8px; padding: 16px;");
    auto* warnLayout = new QVBoxLayout(warn);
    auto* warnTitle = new QLabel("\xe2\x9a\xa0 Experimental");
    warnTitle->setStyleSheet("color: #f59e0b; font-weight: 600;");
    warnLayout->addWidget(warnTitle);
    auto* warnText = new QLabel("Custom UDP Protocol enables zero-RTT transport with reduced latency. "
                                 "Kill Switch blocks all traffic if the custom protocol fails. "
                                 "These features are experimental and may affect stability.");
    warnText->setWordWrap(true); warnText->setStyleSheet("color: #d4a246;");
    warnLayout->addWidget(warnText);
    layout->addWidget(warn);

    layout->addStretch();
}

QWidget* AdvancePage::createTransportSection() {
    auto* group = new QGroupBox("Transport"); auto* layout = new QVBoxLayout(group);
    m_customUdpCheck = new QCheckBox("Custom UDP Protocol + Zero RTT");
    m_killSwitchCheck = new QCheckBox("Kill Switch");
    layout->addWidget(m_customUdpCheck); layout->addWidget(m_killSwitchCheck);
    return group;
}

QWidget* AdvancePage::createRolloutSection() {
    auto* group = new QGroupBox("Rollout"); auto* form = new QFormLayout(group);
    m_canaryPercentSpin = new QSpinBox(); m_canaryPercentSpin->setRange(0, 100); m_canaryPercentSpin->setValue(100);
    form->addRow("CANARY PERCENT", m_canaryPercentSpin);
    return group;
}

void AdvancePage::loadConfig() {
    auto cfg = Application::instance().configManager().config();
    m_customUdpCheck->setChecked(cfg.customUdpProtocol);
    m_killSwitchCheck->setChecked(cfg.killSwitch);
    m_canaryPercentSpin->setValue(static_cast<int>(cfg.canaryPercent));
}

void AdvancePage::saveConfig() {
    auto cfg = Application::instance().configManager().config();
    cfg.customUdpProtocol = m_customUdpCheck->isChecked();
    cfg.killSwitch = m_killSwitchCheck->isChecked();
    cfg.canaryPercent = static_cast<uint32_t>(m_canaryPercentSpin->value());
    Application::instance().configManager().updateConfig(cfg);
    Application::instance().configManager().save();
}

} // namespace ProxyBridge
