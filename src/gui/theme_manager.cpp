#include "gui/theme_manager.h"
#include <QApplication>

namespace ProxyBridge {

ThemeManager::ThemeManager(QObject* parent) : QObject(parent) { buildPalette(); }
ThemeManager::~ThemeManager() = default;

void ThemeManager::applyDarkTheme() {
    buildPalette();
    qApp->setStyleSheet(generateStyleSheet());
}

const ThemePalette& ThemeManager::palette() const { return m_palette; }

void ThemeManager::buildPalette() {
    m_palette.background = QColor(24, 24, 27);
    m_palette.surface = QColor(32, 32, 36);
    m_palette.surfaceVariant = QColor(42, 42, 46);
    m_palette.primary = QColor(245, 158, 11);    // Orange accent (like YBridge)
    m_palette.primaryHover = QColor(217, 119, 6);
    m_palette.accent = QColor(245, 158, 11);
    m_palette.accentHover = QColor(251, 191, 36);
    m_palette.text = QColor(228, 228, 231);
    m_palette.textSecondary = QColor(130, 130, 140);
    m_palette.border = QColor(55, 55, 60);
    m_palette.success = QColor(34, 197, 94);
    m_palette.warning = QColor(234, 179, 8);
    m_palette.error = QColor(239, 68, 68);
    m_palette.sidebar = QColor(17, 17, 20);
    m_palette.input = QColor(24, 24, 28);
    m_palette.card = QColor(32, 32, 36);
}

QString ThemeManager::generateStyleSheet() const {
    auto bg = m_palette.background.name();
    auto surface = m_palette.surface.name();
    auto primary = m_palette.primary.name();
    auto primaryHover = m_palette.primaryHover.name();
    auto text = m_palette.text.name();
    auto textSec = m_palette.textSecondary.name();
    auto border = m_palette.border.name();
    auto sidebar = m_palette.sidebar.name();
    auto input = m_palette.input.name();
    auto card = m_palette.card.name();
    auto success = m_palette.success.name();
    auto error = m_palette.error.name();

    return QString(R"(
        * { font-family: 'Segoe UI', sans-serif; font-size: 13px; }
        QMainWindow { background-color: %1; }
        QWidget { background-color: transparent; color: %2; }
        QWidget#sidebar { background-color: %3; border-right: 1px solid %4; }
        QWidget#contentArea { background-color: %1; }
        QWidget#card { background-color: %5; border: 1px solid %4; border-radius: 8px; padding: 16px; }
        QWidget#logPanel { background-color: %3; border-top: 1px solid %4; }

        QPushButton {
            background-color: %6; color: %2; border: 1px solid %4;
            border-radius: 6px; padding: 8px 16px; font-weight: 500;
        }
        QPushButton:hover { background-color: %7; }
        QPushButton#primaryButton, QPushButton#startButton {
            background-color: %8; color: white; border: none; font-weight: 600;
        }
        QPushButton#primaryButton:hover, QPushButton#startButton:hover { background-color: %9; }
        QPushButton#stopButton { background-color: %10; color: white; border: none; }
        QPushButton#navButton {
            background-color: transparent; border: none; border-radius: 6px;
            padding: 10px 16px; text-align: left; color: %11;
        }
        QPushButton#navButton:hover { background-color: %6; }
        QPushButton#navButton[selected="true"] { background-color: %6; color: %8; font-weight: 600; }

        QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QComboBox {
            background-color: %12; color: %2; border: 1px solid %4;
            border-radius: 6px; padding: 8px 12px;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border-color: %8; }
        QComboBox::drop-down { border: none; width: 30px; }
        QComboBox QAbstractItemView { background-color: %5; border: 1px solid %4; }

        QCheckBox { color: %2; spacing: 8px; }
        QCheckBox::indicator { width: 40px; height: 22px; border-radius: 11px; background-color: %4; }
        QCheckBox::indicator:checked { background-color: %8; }

        QGroupBox { background-color: %5; border: 1px solid %4; border-radius: 8px; margin-top: 12px; padding-top: 20px; }
        QGroupBox::title { subcontrol-origin: margin; left: 16px; padding: 0 8px; font-weight: 600; }

        QListWidget { background-color: %5; border: 1px solid %4; border-radius: 6px; }
        QListWidget::item { padding: 8px; border-bottom: 1px solid %4; }
        QListWidget::item:selected { background-color: %6; color: %8; }

        QScrollBar:vertical { background: transparent; width: 8px; }
        QScrollBar::handle:vertical { background: %4; border-radius: 4px; min-height: 30px; }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0; }

        QLabel#statusGreen { color: %13; }
        QLabel#statusRed { color: %10; }
        QLabel#title { font-size: 18px; font-weight: 700; }
        QLabel#subtitle { font-size: 13px; color: %11; }
        QLabel#sectionTitle { font-size: 14px; font-weight: 600; }
    )")
    .arg(bg).arg(text).arg(sidebar).arg(border).arg(card)
    .arg(surface).arg(m_palette.surfaceVariant.name())
    .arg(primary).arg(primaryHover).arg(error)
    .arg(textSec).arg(input).arg(success);
}

} // namespace ProxyBridge
