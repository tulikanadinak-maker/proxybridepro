#include "gui/license_page.h"
#include <QGroupBox>
#include <QFormLayout>
#include <QFrame>
#include <QMessageBox>

namespace ProxyBridge {

LicensePage::LicensePage(QWidget* parent) : QWidget(parent) { setupUi(); updateStatus(); }
LicensePage::~LicensePage() = default;

void LicensePage::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 32, 32, 32); layout->setSpacing(16);

    auto* title = new QLabel("License");
    title->setObjectName("title"); layout->addWidget(title);
    auto* subtitle = new QLabel("Manage your license key and activation status.");
    subtitle->setObjectName("subtitle"); layout->addWidget(subtitle);

    // Status section
    auto* statusGroup = new QGroupBox("License Status");
    auto* statusForm = new QFormLayout(statusGroup);
    m_statusLabel = new QLabel("No license"); statusForm->addRow("Status:", m_statusLabel);
    m_expiresLabel = new QLabel("N/A"); statusForm->addRow("Expires:", m_expiresLabel);
    m_verifiedLabel = new QLabel("N/A"); statusForm->addRow("Verified:", m_verifiedLabel);
    layout->addWidget(statusGroup);

    // Activation
    auto* actGroup = new QGroupBox("Activation");
    auto* actForm = new QFormLayout(actGroup);
    m_licenseKeyEdit = new QLineEdit(); m_licenseKeyEdit->setPlaceholderText("Enter license key...");
    actForm->addRow("LICENSE KEY", m_licenseKeyEdit);
    m_deviceIdLabel = new QLabel("Loading...");
    m_deviceIdLabel->setStyleSheet("font-family: 'Consolas'; font-size: 11px; color: #82828c;");
    actForm->addRow("DEVICE ID", m_deviceIdLabel);
    layout->addWidget(actGroup);

    m_activateButton = new QPushButton("Activate License");
    m_activateButton->setObjectName("primaryButton");
    m_activateButton->setMinimumHeight(40);
    m_activateButton->setStyleSheet("background-color: #dc2626; color: white; border: none; border-radius: 6px; font-weight: 600;");
    connect(m_activateButton, &QPushButton::clicked, this, &LicensePage::onActivate);
    layout->addWidget(m_activateButton);
    layout->addStretch();
}

void LicensePage::updateStatus() {
    // In a real app, this would check license validity
    m_statusLabel->setText("Open Source - No License Required");
    m_statusLabel->setStyleSheet("color: #22c55e; font-weight: 600;");
    m_expiresLabel->setText("Never");
    m_verifiedLabel->setText("N/A");

    // Generate device ID from machine info
    m_deviceIdLabel->setText("PROXYBRIDGE-PRO-OPEN-SOURCE-BUILD");
}

void LicensePage::onActivate() {
    QMessageBox::information(this, "License",
        "ProxyBridge Pro is open source. No license required!");
}

} // namespace ProxyBridge
