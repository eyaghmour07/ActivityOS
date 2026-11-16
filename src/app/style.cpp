#include "style.hpp"

namespace activityos::ui {

QString friendlyStyleSheet(const QPalette& palette) {
    Q_UNUSED(palette);
    const QString background = "#0C0D11";
    const QString surface = "#13151C";
    const QString surfaceAlt = "#161820";
    const QString secondary = "#1A1D27";
    const QString border = "#12FFFFFF";
    const QString text = "#E4E6ED";
    const QString muted = "#5C6478";
    const QString accent = "#00DFA2";
    const QString accentSoft = "#1A00DFA2";
    const QString selection = "#2600DFA2";
    const QString accentText = "#060809";

    QString style = R"(
        * { color: @TEXT; }
        QMainWindow, QDialog { background: @BG; color: @TEXT; }
        #pageScroll { background: @BG; border: 0; }
        #pageContent { background: @BG; }
        #sidebar {
            background: @SURFACE;
            border-right: 1px solid @BORDER;
        }
        #sidebarHeader {
            background: @SURFACE;
            border-bottom: 1px solid @BORDER;
        }
        #logoBadge {
            background: @ACCENT;
            color: @ACCENT_TEXT;
            border-radius: 4px;
            font-size: 11px;
            font-weight: 700;
            min-width: 24px;
            max-width: 24px;
            min-height: 24px;
            max-height: 24px;
            qproperty-alignment: AlignCenter;
        }
        #brand {
            font-size: 14px;
            font-weight: 600;
            color: @TEXT;
            letter-spacing: -0.2px;
        }
        #navigation { border: 0; background: transparent; outline: 0; padding: 4px; }
        #navigation::item {
            color: @MUTED;
            min-height: 28px;
            padding: 10px 12px;
            margin: 2px 0;
            border-radius: 4px;
            border-left: 2px solid transparent;
        }
        #navigation::item:hover { background: @SECONDARY; color: @TEXT; }
        #navigation::item:selected {
            background: @ACCENT_SOFT;
            color: @ACCENT;
            border-left: 2px solid @ACCENT;
            font-weight: 600;
        }
        #trayStatusBox {
            background: @SURFACE_ALT;
            border-radius: 4px;
            padding: 12px;
        }
        #trayStatusLabel { color: @ACCENT; font-size: 11px; font-weight: 600; }
        #trayStatusDetail { color: @MUTED; font-size: 11px; }
        #monoCaption {
            color: @MUTED;
            font-size: 11px;
            font-weight: 600;
            letter-spacing: 1.2px;
        }
        #pageTitle {
            color: @TEXT;
            font-size: 22px;
            font-weight: 600;
            letter-spacing: -0.3px;
        }
        #recordingDot { background: @ACCENT; border-radius: 4px; }
        #recordingLabel {
            color: @ACCENT;
            font-size: 11px;
            font-weight: 600;
            letter-spacing: 0.8px;
            background: transparent;
        }
        #sidebarTrayFrame { background: @SURFACE; border-top: 1px solid @BORDER; }
        #trayStatusBox { background: @SURFACE_ALT; border-radius: 4px; }
        #appBreakdownName {
            font-size: 14px;
            font-weight: 500;
            color: @TEXT;
            background: transparent;
            border: 0;
            padding: 0;
        }
        #appBreakdownMinutes {
            color: @MUTED;
            font-size: 12px;
            background: transparent;
            border: 0;
            padding: 0;
        }
        #timelineScroll { background: transparent; }
        #filterButton {
            color: @MUTED;
            background: @SECONDARY;
            border: 0;
            border-radius: 4px;
            padding: 6px 14px;
            font-size: 11px;
            font-weight: 600;
        }
        #filterButton:checked {
            color: @ACCENT_TEXT;
            background: @ACCENT;
        }
        #trackingPill {
            color: @MUTED;
            background: @SECONDARY;
            border: 1px solid @BORDER;
            border-radius: 4px;
            padding: 6px 10px;
            font-size: 11px;
            font-weight: 600;
        }
        #activeApp { color: @MUTED; font-size: 12px; }
        #metricCard, #metricCardAccent, #sectionCard {
            color: @TEXT;
            background: @SURFACE;
            border: 1px solid @BORDER;
            border-radius: 6px;
        }
        #metricValue {
            color: @TEXT;
            font-size: 22px;
            font-weight: 600;
            min-height: 28px;
        }
        #metricCardAccent #metricValue { color: @ACCENT; }
        #metricSub {
            color: @MUTED;
            font-size: 11px;
            line-height: 14px;
        }
        #pageOuter, #pageContent, #pages { background: @BG; }
        #settingsRow { background: transparent; border-bottom: 1px solid @BORDER; }
        #settingsTitle { color: @TEXT; font-size: 14px; font-weight: 600; }
        #settingsSection {
            color: @MUTED;
            background: @SURFACE_ALT;
            font-size: 11px;
            font-weight: 600;
            letter-spacing: 1.2px;
        }
        #goalTitle { color: @TEXT; font-size: 14px; font-weight: 600; }
        #statusBadge {
            color: @ACCENT;
            background: @ACCENT_SOFT;
            border-radius: 4px;
            padding: 4px 8px;
            font-size: 11px;
            font-weight: 600;
        }
        #statusBadgeTrack {
            color: #7B8CDE;
            background: #227B8CDE;
            border-radius: 4px;
            padding: 4px 8px;
            font-size: 11px;
            font-weight: 600;
        }
        QProgressBar {
            background: @SURFACE_ALT;
            border: 0;
            border-radius: 4px;
            min-height: 8px;
            max-height: 8px;
        }
        QProgressBar::chunk { background: @ACCENT; border-radius: 4px; }
        #dangerButton {
            color: #EF4444;
            background: #14EF4444;
            border: 1px solid #44EF4444;
        }
        #dangerButton:hover { background: #22EF4444; }
        QPushButton {
            color: @TEXT;
            background: @SECONDARY;
            border: 1px solid @BORDER;
            border-radius: 4px;
            padding: 8px 14px;
            font-weight: 600;
        }
        QPushButton:hover { border-color: @ACCENT; background: @ACCENT_SOFT; color: @TEXT; }
        QPushButton:pressed { background: @SELECTION; }
        QPushButton:disabled { color: @MUTED; border-color: @BORDER; }
        #primaryButton {
            color: @ACCENT_TEXT;
            background: @ACCENT;
            border: 1px solid @ACCENT;
        }
        #primaryButton:hover { background: #00F0B0; border-color: #00F0B0; color: @ACCENT_TEXT; }
        QTableWidget, QListWidget {
            color: @TEXT;
            background: transparent;
            alternate-background-color: @SECONDARY;
            border: 0;
            outline: 0;
        }
        QTableWidget::item, QListWidget::item { color: @TEXT; padding: 4px; }
        QTableWidget::item:selected, QListWidget::item:selected {
            color: @TEXT;
            background: @SELECTION;
        }
        #insightList { padding: 0; background: transparent; border: 0; }
        #insightList::item {
            color: @TEXT;
            background: @SECONDARY;
            border-radius: 4px;
            margin: 0 0 8px 0;
            padding: 12px 14px;
        }
        #insightList::item:selected { color: @TEXT; background: @SELECTION; }
        QHeaderView::section {
            color: @MUTED;
            background: transparent;
            border: 0;
            border-bottom: 1px solid @BORDER;
            padding: 10px 8px;
            font-size: 11px;
            font-weight: 600;
            letter-spacing: 0.8px;
        }
        QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            color: @TEXT;
            background: @SURFACE;
            selection-color: @TEXT;
            selection-background-color: @SELECTION;
            border: 1px solid @BORDER;
            border-radius: 4px;
            padding: 8px;
        }
        QComboBox QAbstractItemView {
            color: @TEXT;
            background: @SURFACE;
            selection-color: @TEXT;
            selection-background-color: @SELECTION;
            border: 1px solid @BORDER;
        }
        QCheckBox { color: @TEXT; spacing: 8px; }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid @BORDER;
            border-radius: 4px;
            background: @SECONDARY;
        }
        QCheckBox::indicator:checked { background: @ACCENT; border-color: @ACCENT; }
        QMenu {
            color: @TEXT;
            background: @SURFACE;
            border: 1px solid @BORDER;
            padding: 5px;
        }
        QMenu::item { color: @TEXT; padding: 7px 22px; border-radius: 4px; }
        QMenu::item:selected { color: @TEXT; background: @SELECTION; }
        QToolTip {
            color: @TEXT;
            background: @SURFACE;
            border: 1px solid @BORDER;
            padding: 6px;
        }
        QMessageBox QLabel { color: @TEXT; background: transparent; }
        QStatusBar { color: @MUTED; background: @SURFACE; border-top: 1px solid @BORDER; }
        QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; }
        QScrollBar::handle:vertical {
            background: @SECONDARY;
            min-height: 28px;
            border-radius: 4px;
        }
        QScrollBar:horizontal { background: transparent; height: 6px; margin: 0; }
        QScrollBar::handle:horizontal {
            background: @SECONDARY;
            min-width: 24px;
            border-radius: 3px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
        QFormLayout QLabel { color: @MUTED; }
    )";
    style.replace("@ACCENT_TEXT", accentText);
    style.replace("@ACCENT_SOFT", accentSoft);
    style.replace("@SECONDARY", secondary);
    style.replace("@SURFACE_ALT", surfaceAlt);
    style.replace("@SELECTION", selection);
    style.replace("@SURFACE", surface);
    style.replace("@BORDER", border);
    style.replace("@ACCENT", accent);
    style.replace("@MUTED", muted);
    style.replace("@TEXT", text);
    style.replace("@BG", background);
    return style;
}

} // namespace activityos::ui
