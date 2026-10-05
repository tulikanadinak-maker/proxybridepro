#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>

namespace ProxyBridge {

class ProxyPage : public QWidget {
    Q_OBJECT

public:
    explicit ProxyPage(QWidget* parent = nullptr);
    ~ProxyPage() override;

    void loadConfig();
    void saveConfig();

private slots:
    void onRotateIp();
    void onResetIp();

private:
    void setupUi();
    QWidget* createNetworkSection();
    QWidget* createAuthSection();
    QWidget* createBindingSection();

    // Network
    QComboBox* m_modeCombo{nullptr};
    QCheckBox* m_multiSourceCheck{nullptr};

    // Auth
    QLineEdit* m_passwordEdit{nullptr};
    QSpinBox* m_ipCountSpin{nullptr};

    // Binding
    QLineEdit* m_hostEdit{nullptr};
    QSpinBox* m_portSpin{nullptr};

    // Buttons
    QPushButton* m_rotateButton{nullptr};
    QPushButton* m_resetButton{nullptr};
};

} // namespace ProxyBridge
