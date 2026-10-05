#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>

namespace ProxyBridge {

class AdvancePage : public QWidget {
    Q_OBJECT

public:
    explicit AdvancePage(QWidget* parent = nullptr);
    ~AdvancePage() override;

    void loadConfig();
    void saveConfig();

private:
    void setupUi();
    QWidget* createTransportSection();
    QWidget* createRolloutSection();

    QCheckBox* m_customUdpCheck{nullptr};
    QCheckBox* m_killSwitchCheck{nullptr};
    QSpinBox* m_canaryPercentSpin{nullptr};
};

} // namespace ProxyBridge
