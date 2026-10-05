#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QTextEdit>

namespace ProxyBridge {

class FrpPage : public QWidget {
    Q_OBJECT

public:
    explicit FrpPage(QWidget* parent = nullptr);
    ~FrpPage() override;

    void loadConfig();
    void saveConfig();
    void updateConnectionInfo(const QString& info);

private:
    void setupUi();
    QWidget* createServerSection();
    QWidget* createPortsSection();
    QWidget* createInfoSection();

    QCheckBox* m_enableCheck{nullptr};
    QLineEdit* m_vpsHostEdit{nullptr};
    QSpinBox* m_serverPortSpin{nullptr};
    QLineEdit* m_tokenEdit{nullptr};
    QSpinBox* m_proxyPortSpin{nullptr};
    QSpinBox* m_dashboardPortSpin{nullptr};
    QLabel* m_connectionInfoLabel{nullptr};
};

} // namespace ProxyBridge
