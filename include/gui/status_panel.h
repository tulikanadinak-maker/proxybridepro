#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>

namespace ProxyBridge {

class StatusPanel : public QWidget {
    Q_OBJECT

public:
    explicit StatusPanel(QWidget* parent = nullptr);
    ~StatusPanel() override;

    void setStatus(const QString& status);
    void setSlotInfo(const QString& info);
    void setRotationInfo(const QString& info);

private:
    void setupUi();

    QLabel* m_statusLabel{nullptr};
    QLabel* m_slotInfoLabel{nullptr};
    QLabel* m_rotationLabel{nullptr};
};

} // namespace ProxyBridge
