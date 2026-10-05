#include "gui/log_panel.h"
#include <QHBoxLayout>
#include <QDateTime>
#include <QScrollBar>

namespace ProxyBridge {

LogPanel::LogPanel(QWidget* parent) : QWidget(parent) { setObjectName("logPanel"); setupUi(); }
LogPanel::~LogPanel() = default;

void LogPanel::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* header = new QHBoxLayout();
    header->setContentsMargins(16, 4, 16, 4);

    auto* liveIcon = new QLabel("\xe2\x97\x8f");
    liveIcon->setStyleSheet("color: #ef4444; font-size: 10px;");
    header->addWidget(liveIcon);

    auto* liveText = new QLabel("Live Log");
    liveText->setStyleSheet("font-size: 12px; font-weight: 600;");
    header->addWidget(liveText);

    header->addStretch();

    m_liveLabel = new QLabel("Connected: 0 | Live Status: Off");
    m_liveLabel->setStyleSheet("font-size: 11px; color: #82828c;");
    header->addWidget(m_liveLabel);

    layout->addLayout(header);

    m_logText = new QTextEdit();
    m_logText->setReadOnly(true);
    m_logText->setStyleSheet(
        "background-color: #111114; border: none; "
        "font-family: 'Consolas', monospace; font-size: 11px; "
        "color: #a0a0a8; padding: 8px;");
    m_logText->append("ProxyBridge is ready. Set your slots and start the service.");

    layout->addWidget(m_logText);
}

void LogPanel::appendLog(const QString& message) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_logText->append("[" + timestamp + "] " + message);
    if (m_autoScroll)
        m_logText->verticalScrollBar()->setValue(m_logText->verticalScrollBar()->maximum());
}

void LogPanel::clear() { m_logText->clear(); }

void LogPanel::setLiveStatus(bool online) {
    m_isOnline = online;
    QString status = online ? "On" : "Off";
    QString color = online ? "#22c55e" : "#82828c";
    m_liveLabel->setText(QString("Connected: %1 | Live Status: %2").arg(m_connectedCount).arg(status));
    m_liveLabel->setStyleSheet("font-size: 11px; color: " + color + ";");
}

void LogPanel::setConnectedCount(int count) {
    m_connectedCount = count;
    QString status = m_isOnline ? "On" : "Off";
    QString color = m_isOnline ? "#22c55e" : "#82828c";
    m_liveLabel->setText(QString("Connected: %1 | Live Status: %2").arg(count).arg(status));
    m_liveLabel->setStyleSheet("font-size: 11px; color: " + color + ";");
}

} // namespace ProxyBridge
