#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>

namespace ProxyBridge {

class LogPanel : public QWidget {
    Q_OBJECT

public:
    explicit LogPanel(QWidget* parent = nullptr);
    ~LogPanel() override;

    void appendLog(const QString& message);
    void clear();
    void setLiveStatus(bool online);
    void setConnectedCount(int count);

private:
    void setupUi();

    QTextEdit* m_logText{nullptr};
    QLabel* m_liveLabel{nullptr};
    QPushButton* m_clearButton{nullptr};
    bool m_autoScroll{true};
    bool m_isOnline{false};
    int m_connectedCount{0};
};

} // namespace ProxyBridge
