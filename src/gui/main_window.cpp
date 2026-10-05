#include "gui/main_window.h"
#include "gui/sidebar.h"
#include "gui/status_panel.h"
#include "gui/log_panel.h"
#include "gui/proxy_page.h"
#include "gui/advance_page.h"
#include "gui/frp_page.h"
#include "gui/sources_page.h"
#include "gui/license_page.h"
#include "gui/theme_manager.h"
#include "core/application.h"
#include "config/config_manager.h"
#include "log/log_manager.h"
#include "proxy/proxy_server.h"
#include "network/connection_manager.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QCloseEvent>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QMetaObject>
#include <QTimer>

#ifdef interface
#undef interface
#endif

namespace ProxyBridge {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("ProxyBridge Pro v2");
    setMinimumSize(1100, 650);
    m_themeManager = std::make_unique<ThemeManager>(this);
    m_themeManager->applyDarkTheme();
    setupUi(); setupTrayIcon(); setupConnections();

    // Timer to update connected client count every second
    auto* statsTimer = new QTimer(this);
    connect(statsTimer, &QTimer::timeout, [this]() {
        auto& app = Application::instance();
        if (app.proxyServer().isRunning()) {
            int count = static_cast<int>(app.proxyServer().connectionManager().activeCount());
            m_logPanel->setConnectedCount(count);
        }
    });
    statsTimer->start(1000);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi() {
    auto* central = new QWidget(this); setCentralWidget(central);
    auto* mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0); mainLayout->setSpacing(0);

    m_sidebar = std::make_unique<Sidebar>(central);
    mainLayout->addWidget(m_sidebar.get());

    auto* rightPanel = new QWidget(central);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0); rightLayout->setSpacing(0);

    m_contentStack = new QStackedWidget(rightPanel);
    m_contentStack->setObjectName("contentArea");

    m_proxyPage = new ProxyPage(m_contentStack);
    m_advancePage = new AdvancePage(m_contentStack);
    m_frpPage = new FrpPage(m_contentStack);
    m_sourcesPage = new SourcesPage(m_contentStack);
    m_licensePage = new LicensePage(m_contentStack);

    m_contentStack->addWidget(m_proxyPage);
    m_contentStack->addWidget(m_advancePage);
    m_contentStack->addWidget(m_frpPage);
    m_contentStack->addWidget(m_sourcesPage);
    m_contentStack->addWidget(m_licensePage);

    rightLayout->addWidget(m_contentStack, 1);

    // Log panel at bottom
    m_logPanel = std::make_unique<LogPanel>(rightPanel);
    m_logPanel->setMaximumHeight(150);
    rightLayout->addWidget(m_logPanel.get());

    mainLayout->addWidget(rightPanel, 1);
}

void MainWindow::setupTrayIcon() {
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setToolTip("ProxyBridge Pro");
    m_trayMenu = new QMenu(this);
    m_trayMenu->addAction("Show", this, &QMainWindow::show);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction("Quit", qApp, &QApplication::quit);
    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->show();
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);
}

void MainWindow::setupConnections() {
    connect(m_sidebar.get(), &Sidebar::pageChanged, this, &MainWindow::onPageChanged);
    connect(m_sidebar.get(), &Sidebar::startClicked, this, &MainWindow::onStartClicked);
    connect(m_sidebar.get(), &Sidebar::stopClicked, this, &MainWindow::onStopClicked);
}

void MainWindow::onPageChanged(int index) { m_contentStack->setCurrentIndex(index); }

void MainWindow::onStartClicked() {
    auto& app = Application::instance();

    // Save current UI config before starting
    if (m_proxyPage) m_proxyPage->saveConfig();

    // Hook log callback to Live Log panel
    app.logManager().setCallback([this](const ProxyBridge::LogEntry& entry) {
        QString msg = QString::fromStdString(entry.message);
        QMetaObject::invokeMethod(this, [this, msg]() {
            m_logPanel->appendLog(msg);
        }, Qt::QueuedConnection);
    });

    bool ok = app.startProxy();
    if (ok) {
        m_logPanel->appendLog("Waiting for connections...");
        m_logPanel->setLiveStatus(true);
    } else {
        m_logPanel->appendLog("ERROR: Failed to start proxy server!");
        m_logPanel->appendLog("Check if port is already in use or run as Administrator.");
        m_logPanel->setLiveStatus(false);
    }
}

void MainWindow::onStopClicked() {
    Application::instance().stopProxy();
    m_logPanel->appendLog("Proxy server stopped.");
    m_logPanel->setLiveStatus(false);
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::DoubleClick) { show(); activateWindow(); }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    event->ignore(); hide();
    m_trayIcon->showMessage("ProxyBridge Pro", "Running in background", QSystemTrayIcon::Information, 1500);
}

void MainWindow::updateStatus() {}

} // namespace ProxyBridge
