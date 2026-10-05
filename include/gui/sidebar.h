#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QVector>
#include <QString>

namespace ProxyBridge {

struct NavItem {
    QString text;
    QString icon;
    int pageIndex;
};

class Sidebar : public QWidget {
    Q_OBJECT

public:
    explicit Sidebar(QWidget* parent = nullptr);
    ~Sidebar() override;

    void setCurrentIndex(int index);
    int currentIndex() const;

signals:
    void pageChanged(int index);
    void startClicked();
    void stopClicked();

private:
    void setupUi();
    void updateButtonStyles();
    QPushButton* createNavButton(const NavItem& item);

    QVBoxLayout* m_layout{nullptr};
    QVector<QPushButton*> m_navButtons;
    QVector<NavItem> m_items;
    QPushButton* m_startButton{nullptr};
    QPushButton* m_stopButton{nullptr};
    int m_currentIndex{0};
};

} // namespace ProxyBridge
