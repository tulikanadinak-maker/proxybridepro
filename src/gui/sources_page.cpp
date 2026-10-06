#include "gui/sources_page.h"
#include "core/application.h"
#include "ipv6/ipv6_manager.h"
#include "log/log_manager.h"
#include <QGroupBox>
#include <QFormLayout>
#include <QMessageBox>
#include <string>
#include <set>

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>
#endif

#ifdef interface
#undef interface
#endif

namespace ProxyBridge {

static QComboBox* s_prefixCombo = nullptr;

SourcesPage::SourcesPage(QWidget* parent) : QWidget(parent) { setupUi(); }
SourcesPage::~SourcesPage() = default;

void SourcesPage::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    auto* title = new QLabel("Sources");
    title->setObjectName("title");
    layout->addWidget(title);
    auto* subtitle = new QLabel("Manage network interfaces and IPv6 prefix sources.");
    subtitle->setObjectName("subtitle");
    layout->addWidget(subtitle);

    layout->addWidget(createSourceList());
    layout->addWidget(createEditor());

    auto* btnLayout = new QHBoxLayout();
    m_addButton = new QPushButton("Add");
    btnLayout->addWidget(m_addButton);
    m_applyButton = new QPushButton("Apply");
    m_applyButton->setObjectName("primaryButton");
    btnLayout->addWidget(m_applyButton);
    m_removeButton = new QPushButton("Remove");
    m_removeButton->setStyleSheet("background-color: #7f1d1d; color: white; border: 1px solid #ef4444;");
    btnLayout->addWidget(m_removeButton);

    connect(m_addButton, &QPushButton::clicked, this, &SourcesPage::onAdd);
    connect(m_applyButton, &QPushButton::clicked, this, &SourcesPage::onApply);
    connect(m_removeButton, &QPushButton::clicked, this, &SourcesPage::onRemove);
    layout->addLayout(btnLayout);
    layout->addStretch();
}

QWidget* SourcesPage::createSourceList() {
    m_sourceList = new QListWidget();
    m_sourceList->setMaximumHeight(100);
    connect(m_sourceList, &QListWidget::currentRowChanged, this, &SourcesPage::onSourceSelected);
    return m_sourceList;
}

QWidget* SourcesPage::createEditor() {
    auto* group = new QGroupBox("Editor");
    auto* form = new QFormLayout(group);
    form->setSpacing(10);

    m_nameEdit = new QLineEdit("Source 1");
    form->addRow("NAME", m_nameEdit);

    m_modeCombo = new QComboBox();
    m_modeCombo->addItems({"Auto", "Manual"});
    form->addRow("MODE", m_modeCombo);

    // Interface dropdown - auto detect
    m_interfaceCombo = new QComboBox();
    m_interfaceCombo->setEditable(true);
#ifdef _WIN32
    {
        ULONG bufLen = 15000;
        auto* addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(malloc(bufLen));
        if (addrs) {
            DWORD res = GetAdaptersAddresses(AF_INET6, GAA_FLAG_INCLUDE_PREFIX, nullptr, addrs, &bufLen);
            if (res == ERROR_BUFFER_OVERFLOW) {
                free(addrs);
                addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(malloc(bufLen));
                if (addrs) res = GetAdaptersAddresses(AF_INET6, GAA_FLAG_INCLUDE_PREFIX, nullptr, addrs, &bufLen);
            }
            if (res == NO_ERROR && addrs) {
                for (auto* a = addrs; a; a = a->Next) {
                    if (a->OperStatus != IfOperStatusUp) continue;
                    char name[256];
                    WideCharToMultiByte(CP_UTF8, 0, a->FriendlyName, -1, name, sizeof(name), nullptr, nullptr);
                    m_interfaceCombo->addItem(QString::fromUtf8(name));
                }
            }
            if (addrs) free(addrs);
        }
    }
#endif
    if (m_interfaceCombo->count() == 0) m_interfaceCombo->addItem("WiFi");
    form->addRow("INTERFACE", m_interfaceCombo);

    // IPv6 PREFIX - editable combo with detected prefixes
    m_prefixEdit = new QLineEdit();
    m_prefixEdit->setPlaceholderText("e.g. 2400:9800:9b3:fdc5::");

    // Detect prefixes and add to a combo
    auto* prefixCombo = new QComboBox();
    prefixCombo->setEditable(true);
    prefixCombo->setMinimumWidth(300);

    // Auto-detect IPv6 prefixes
    auto detected = IPv6Manager::detectAllSubnets();
    for (const auto& sub : detected) {
        QString item = QString::fromStdString(sub.prefix.toString()) +
                       "/" + QString::number(sub.prefixLength) +
                       " (" + QString::fromStdString(sub.ifaceName) + ")";
        prefixCombo->addItem(item);
    }

    if (prefixCombo->count() == 0) {
        prefixCombo->addItem("No IPv6 prefix detected - type manually");
    }

    // When user selects from combo, extract prefix into m_prefixEdit
    connect(prefixCombo, &QComboBox::currentTextChanged, [this](const QString& text) {
        // Extract IP part: everything before the first /
        QString prefix = text.split("/").first().trimmed();
        // Remove parenthetical info
        if (prefix.contains(" (")) prefix = prefix.split(" (").first().trimmed();
        if (!prefix.startsWith("No ")) {
            m_prefixEdit->setText(prefix);
        }
        // Auto-match the interface to the prefix's origin interface so the
        // user never has to pick it manually (prevents mismatched bindings).
        if (text.contains(" (") && text.endsWith(")")) {
            QString originIface = text.section(" (", -1).chopped(1).trimmed();
            if (!originIface.isEmpty()) {
                int idx = m_interfaceCombo->findText(originIface, Qt::MatchFixedString);
                if (idx >= 0) {
                    m_interfaceCombo->setCurrentIndex(idx);
                } else {
                    m_interfaceCombo->setCurrentText(originIface);
                }
            }
        }
    });

    // Trigger initial selection
    if (prefixCombo->count() > 0 && !detected.empty()) {
        prefixCombo->setCurrentIndex(0);
        QString firstPrefix = QString::fromStdString(detected[0].prefix.toString());
        m_prefixEdit->setText(firstPrefix);
    }

    s_prefixCombo = prefixCombo;
    form->addRow("IPv6 PREFIX", prefixCombo);

    // Also show the extracted prefix (read-only info)
    form->addRow("SELECTED", m_prefixEdit);

    m_prefixLenSpin = new QSpinBox();
    m_prefixLenSpin->setRange(32, 128);
    m_prefixLenSpin->setValue(64);
    form->addRow("PREFIX LENGTH", m_prefixLenSpin);

    m_slotsSpin = new QSpinBox();
    m_slotsSpin->setRange(1, 10000);
    m_slotsSpin->setValue(50);
    form->addRow("SLOTS", m_slotsSpin);

    m_enabledCheck = new QCheckBox("Enabled");
    m_enabledCheck->setChecked(true);
    form->addRow(m_enabledCheck);

    return group;
}

void SourcesPage::refreshSources() {
    m_sourceList->clear();
}

void SourcesPage::onAdd() {
    QString name = m_nameEdit->text();
    QString ifaceName = m_interfaceCombo->currentText();
    QString prefix = m_prefixEdit->text().trimmed();
    int prefixLen = m_prefixLenSpin->value();
    int numSlots = m_slotsSpin->value();

    if (prefix.isEmpty() || prefix.startsWith("No ")) {
        QMessageBox::warning(this, QString("Error"),
            QString("Please select or type a valid IPv6 prefix.\n\n"
                    "Example: 2400:9800:9b3:fdc5::"));
        return;
    }

    // Validate: must contain : and not be link-local
    if (!prefix.contains(":") || prefix.startsWith("fe80") || prefix.startsWith("fd")) {
        QMessageBox::warning(this, QString("Error"),
            QString("Invalid prefix. Must be a global IPv6 prefix (not link-local or ULA).\n\n"
                    "Example: 2400:9800:9b3:fdc5::"));
        return;
    }

    // Strict IPv6 parse - reject garbage before touching the manager
    auto parsedPrefix = IPv6Address::fromString(prefix.toStdString());
    if (!parsedPrefix) {
        QMessageBox::warning(this, QString("Error"),
            QString("Invalid IPv6 prefix format.\n\n"
                    "Example: 2400:9800:9b3:fdc5::"));
        return;
    }

    // Mask off host bits so we store a clean network prefix
    {
        int fullBytes = prefixLen / 8;
        int remainBits = prefixLen % 8;
        for (int i = fullBytes; i < 16; ++i) parsedPrefix->bytes[i] = 0;
        if (remainBits > 0 && fullBytes < 16) {
            uint8_t mask = static_cast<uint8_t>(0xFFu << (8 - remainBits));
            parsedPrefix->bytes[fullBytes] &= mask;
        }
    }

    QString display = name + " | " + m_modeCombo->currentText() + " | " +
                      ifaceName + " | " + prefix + "/" + QString::number(prefixLen) +
                      " | Slots " + QString::number(numSlots) +
                      (m_enabledCheck->isChecked() ? " | Enabled" : " | Disabled");
    m_sourceList->addItem(display);

    std::string logMsg = std::string("Source added: ") + name.toStdString() +
                         " prefix=" + prefix.toStdString() +
                         "/" + std::to_string(prefixLen) +
                         " slots=" + std::to_string(numSlots) +
                         " iface=" + ifaceName.toStdString();
    Application::instance().logManager().log(LogLevel::Info, logMsg, "Sources");

    IPv6Subnet subnet;
    subnet.prefix = *parsedPrefix;
    subnet.prefixLength = prefixLen;
    subnet.ifaceName = ifaceName.toStdString();

    Application::instance().ipv6Manager().initialize(subnet, static_cast<uint32_t>(numSlots));

    std::string initMsg = std::string("IPv6 Manager initialized: prefix=") +
                          prefix.toStdString() + "/" + std::to_string(prefixLen) +
                          " slots=" + std::to_string(numSlots) +
                          " iface=" + ifaceName.toStdString();
    Application::instance().logManager().log(LogLevel::Info, initMsg, "Sources");
}

void SourcesPage::onApply() {
    auto& mgr = Application::instance().ipv6Manager();

    if (mgr.slotCount() == 0) {
        QMessageBox::warning(this, QString("Error"), QString("Add a source first before applying."));
        return;
    }

    Application::instance().logManager().log(LogLevel::Info,
        "Applying source - binding IPv6 addresses to interface...", "Sources");

    bool ok = mgr.start();
    if (ok) {
        std::vector<IPv6Slot> activeSlots = mgr.getActiveSlots();
        int count = static_cast<int>(activeSlots.size());
        std::string msg = std::string("Successfully bound ") + std::to_string(count) +
                          "/" + std::to_string(mgr.slotCount()) + " IPv6 addresses";
        Application::instance().logManager().log(LogLevel::Info, msg, "Sources");

        int limit = (count < 5) ? count : 5;
        for (int i = 0; i < limit; ++i) {
            std::string slotMsg = std::string("  Slot ") + std::to_string(i) +
                                  ": " + activeSlots[static_cast<size_t>(i)].address.toString();
            Application::instance().logManager().log(LogLevel::Info, slotMsg, "Sources");
        }
        if (count > 5) {
            std::string moreMsg = std::string("  ... and ") + std::to_string(count - 5) + " more";
            Application::instance().logManager().log(LogLevel::Info, moreMsg, "Sources");
        }

        if (count == 0) {
            QMessageBox::warning(this, QString("Warning"),
                QString("0 addresses bound! Make sure you run as Administrator."));
        } else {
            QMessageBox::information(this, QString("Success"),
                QString("Bound %1 IPv6 addresses!").arg(count));
        }
    } else {
        Application::instance().logManager().log(LogLevel::Error,
            "Failed to bind IPv6 addresses. Run as Administrator!", "Sources");
        QMessageBox::warning(this, QString("Error"),
            QString("Failed to bind addresses.\nRun as Administrator!"));
    }
}

void SourcesPage::onRemove() {
    int row = m_sourceList->currentRow();
    if (row < 0) return;

    Application::instance().ipv6Manager().stop();
    Application::instance().logManager().log(LogLevel::Info,
        "Source removed, IPv6 addresses unbound", "Sources");
    delete m_sourceList->takeItem(row);
}

void SourcesPage::onSourceSelected(int index) {
    m_selectedSourceId = index;
}

} // namespace ProxyBridge
