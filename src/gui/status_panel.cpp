#include "gui/status_panel.h"

namespace ProxyBridge {

StatusPanel::StatusPanel(QWidget* parent) : QWidget(parent) { setupUi(); }
StatusPanel::~StatusPanel() = default;

void StatusPanel::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8); layout->setSpacing(4);
    m_statusLabel = new QLabel("Status: Stopped"); m_statusLabel->setObjectName("subtitle");
    m_slotInfoLabel = new QLabel("Slots: 0/0"); m_slotInfoLabel->setObjectName("subtitle");
    m_rotationLabel = new QLabel("Rotation: Manual"); m_rotationLabel->setObjectName("subtitle");
    layout->addWidget(m_statusLabel); layout->addWidget(m_slotInfoLabel); layout->addWidget(m_rotationLabel);
}

void StatusPanel::setStatus(const QString& s) { m_statusLabel->setText("Status: " + s); }
void StatusPanel::setSlotInfo(const QString& s) { m_slotInfoLabel->setText("Slots: " + s); }
void StatusPanel::setRotationInfo(const QString& s) { m_rotationLabel->setText("Rotation: " + s); }

} // namespace ProxyBridge
