#pragma once

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QStackedWidget>
#include <memory>

namespace ProxyBridge {

class Sidebar;
class StatusPanel;
class LogPanel;
class ProxyPage;
class AdvancePage;
class FrpPage;
class SourcesPage;
class LicensePage;
class ThemeManager;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onPageChanged(int index);
    void onStartClicked();
    void onStopClicked();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void setupUi();
    void setupTrayIcon();
    void setupConnections();
    void updateStatus();

    std::unique_ptr<Sidebar> m_sidebar;
    std::unique_ptr<StatusPanel> m_statusPanel;
    std::unique_ptr<LogPanel> m_logPanel;
    std::unique_ptr<ThemeManager> m_themeManager;
    QStackedWidget* m_contentStack{nullptr};

    ProxyPage* m_proxyPage{nullptr};
    AdvancePage* m_advancePage{nullptr};
    FrpPage* m_frpPage{nullptr};
    SourcesPage* m_sourcesPage{nullptr};
    LicensePage* m_licensePage{nullptr};

    QSystemTrayIcon* m_trayIcon{nullptr};
    QMenu* m_trayMenu{nullptr};
};

} // namespace ProxyBridge
