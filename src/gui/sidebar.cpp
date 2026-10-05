#include "gui/sidebar.h"
#include <QLabel>
#include <QStyle>

namespace ProxyBridge {

Sidebar::Sidebar(QWidget* parent) : QWidget(parent) {
    setObjectName("sidebar"); setFixedWidth(200); setupUi();
}
Sidebar::~Sidebar() = default;

void Sidebar::setupUi() {
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(8, 16, 8, 16);
    m_layout->setSpacing(4);

    auto* logo = new QLabel("ProxyBridge Pro");
    logo->setStyleSheet("font-size: 14px; font-weight: 700; color: #f59e0b; padding: 8px 12px 20px 12px;");
    m_layout->addWidget(logo);

    // Status section
    auto* statusLabel = new QLabel("STATUS");
    statusLabel->setStyleSheet("font-size: 10px; color: #82828c; padding: 4px 12px; font-weight: 600;");
    m_layout->addWidget(statusLabel);

    auto* statusInfo = new QLabel("Pinned Slot/IP: proxy\nBelum running. IP tidak\nberubah otomatis setelah\nstart kecuali manual\nrotate/reset.");
    statusInfo->setStyleSheet("font-size: 11px; color: #a0a0a8; padding: 4px 12px;");
    statusInfo->setWordWrap(true);
    m_layout->addWidget(statusInfo);

    // Start/Stop buttons
    auto* btnLayout = new QHBoxLayout();
    m_startButton = new QPushButton("Start");
    m_startButton->setObjectName("startButton");
    m_startButton->setCursor(Qt::PointingHandCursor);
    connect(m_startButton, &QPushButton::clicked, this, &Sidebar::startClicked);
    btnLayout->addWidget(m_startButton);

    m_stopButton = new QPushButton("Stop");
    m_stopButton->setObjectName("stopButton");
    m_stopButton->setCursor(Qt::PointingHandCursor);
    connect(m_stopButton, &QPushButton::clicked, this, &Sidebar::stopClicked);
    btnLayout->addWidget(m_stopButton);
    m_layout->addLayout(btnLayout);

    m_layout->addSpacing(16);

    // Nav items
    m_items = {
        {"\xe2\x86\x97 Proxy", "", 0},
        {"\xe2\x9a\x99 Advance", "", 1},
        {"\xe2\x87\x84 FRP", "", 2},
        {"\xe2\x99\xa6 Sources", "", 3},
        {"\xf0\x9f\x94\x91 License", "", 4}
    };

    for (const auto& item : m_items) {
        auto* btn = createNavButton(item);
        m_layout->addWidget(btn);
        m_navButtons.append(btn);
    }

    m_layout->addStretch();

    // Dashboard label at bottom
    auto* dashLabel = new QLabel("DASHBOARD");
    dashLabel->setStyleSheet("font-size: 10px; color: #82828c; padding: 4px 12px; font-weight: 600;");
    m_layout->addWidget(dashLabel);
    auto* dashInfo = new QLabel("Start FRP to generate.");
    dashInfo->setStyleSheet("font-size: 11px; color: #a0a0a8; padding: 4px 12px;");
    m_layout->addWidget(dashInfo);

    updateButtonStyles();
}

QPushButton* Sidebar::createNavButton(const NavItem& item) {
    auto* btn = new QPushButton(item.text);
    btn->setObjectName("navButton");
    btn->setCursor(Qt::PointingHandCursor);
    btn->setMinimumHeight(38);
    connect(btn, &QPushButton::clicked, [this, idx = item.pageIndex]() {
        setCurrentIndex(idx); emit pageChanged(idx);
    });
    return btn;
}

void Sidebar::setCurrentIndex(int index) { m_currentIndex = index; updateButtonStyles(); }
int Sidebar::currentIndex() const { return m_currentIndex; }

void Sidebar::updateButtonStyles() {
    for (int i = 0; i < m_navButtons.size(); ++i) {
        m_navButtons[i]->setProperty("selected", i == m_currentIndex);
        m_navButtons[i]->style()->unpolish(m_navButtons[i]);
        m_navButtons[i]->style()->polish(m_navButtons[i]);
    }
}

} // namespace ProxyBridge
