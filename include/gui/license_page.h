#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

namespace ProxyBridge {

class LicensePage : public QWidget {
    Q_OBJECT

public:
    explicit LicensePage(QWidget* parent = nullptr);
    ~LicensePage() override;

private slots:
    void onActivate();

private:
    void setupUi();
    void updateStatus();

    QLabel* m_statusLabel{nullptr};
    QLabel* m_expiresLabel{nullptr};
    QLabel* m_verifiedLabel{nullptr};
    QLineEdit* m_licenseKeyEdit{nullptr};
    QLabel* m_deviceIdLabel{nullptr};
    QPushButton* m_activateButton{nullptr};
};

} // namespace ProxyBridge
