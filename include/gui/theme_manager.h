#pragma once

#include <QObject>
#include <QString>
#include <QColor>

namespace ProxyBridge {

struct ThemePalette {
    QColor background;
    QColor surface;
    QColor surfaceVariant;
    QColor primary;
    QColor primaryHover;
    QColor accent;       // Orange accent (like YBridge)
    QColor accentHover;
    QColor text;
    QColor textSecondary;
    QColor border;
    QColor success;
    QColor warning;
    QColor error;
    QColor sidebar;
    QColor input;
    QColor card;
};

class ThemeManager : public QObject {
    Q_OBJECT

public:
    explicit ThemeManager(QObject* parent = nullptr);
    ~ThemeManager() override;

    void applyDarkTheme();
    const ThemePalette& palette() const;
    QString generateStyleSheet() const;

private:
    void buildPalette();
    ThemePalette m_palette;
};

} // namespace ProxyBridge
