#include "main_window.hpp"
#include "ui_widgets.hpp"

#include "activityos/analytics.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QSettings>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTableWidget>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <numeric>
#include <unordered_map>

namespace activityos::ui {
namespace {

constexpr std::int64_t kMinuteMs = 60 * 1000;
constexpr std::int64_t kHourMs = 60 * kMinuteMs;
constexpr std::int64_t kDayMs = 24 * kHourMs;

QString duration(std::int64_t milliseconds) {
    const auto totalMinutes = std::max<std::int64_t>(0, milliseconds) / kMinuteMs;
    const auto hours = totalMinutes / 60;
    const auto minutes = totalMinutes % 60;
    return hours > 0 ? QStringLiteral("%1h %2m").arg(hours).arg(minutes)
                     : QStringLiteral("%1m").arg(minutes);
}

QString percent(double value) {
    return QString::number(value * 100.0, 'f', 0) + "%";
}

QLabel* monoCaption(const QString& text) {
    auto* label = new QLabel(text);
    label->setObjectName("monoCaption");
    return label;
}

QLabel* titleLabel(const QString& text) {
    auto* label = new QLabel(text);
    label->setObjectName("pageTitle");
    return label;
}

void equalizeColumns(QGridLayout* grid) {
    for (int column = 0; column < grid->columnCount(); ++column) {
        grid->setColumnStretch(column, 1);
    }
}

QFrame* metricCard(const QString& label, QLabel*& value, QLabel*& sub, bool accent = false) {
    auto* frame = new QFrame;
    frame->setObjectName(accent ? "metricCardAccent" : "metricCard");
    frame->setMinimumHeight(118);
    frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);
    auto* caption = monoCaption(label);
    caption->setWordWrap(true);
    layout->addWidget(caption);
    value = new QLabel("—");
    value->setObjectName("metricValue");
    value->setWordWrap(true);
    value->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    value->setMinimumHeight(34);
    layout->addWidget(value);
    sub = new QLabel;
    sub->setObjectName("metricSub");
    sub->setWordWrap(true);
    layout->addWidget(sub);
    layout->addStretch();
    return frame;
}

QFrame* sectionCard(QWidget* content) {
    auto* frame = new QFrame;
    frame->setObjectName("sectionCard");
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(14);
    layout->addWidget(content);
    return frame;
}

QWidget* wrapSection(const QString& title, QWidget* body) {
    auto* container = new QWidget;
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    if (!title.isEmpty()) layout->addWidget(monoCaption(title));
    layout->addWidget(body);
    return sectionCard(container);
}

QWidget* settingsRow(const QString& title, const QString& description, QWidget* control) {
    auto* row = new QFrame;
    row->setObjectName("settingsRow");
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(16);
    auto* text = new QVBoxLayout;
    text->setSpacing(4);
    auto* heading = new QLabel(title);
    heading->setObjectName("settingsTitle");
    heading->setWordWrap(true);
    auto* detail = new QLabel(description);
    detail->setObjectName("activeApp");
    detail->setWordWrap(true);
    text->addWidget(heading);
    text->addWidget(detail);
    layout->addLayout(text, 1);
    control->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    layout->addWidget(control, 0, Qt::AlignVCenter);
    return row;
}

QLabel* settingsSection(const QString& title) {
    auto* label = monoCaption(title);
    label->setObjectName("settingsSection");
    label->setContentsMargins(20, 14, 20, 10);
    return label;
}

void addPageHeader(QVBoxLayout* layout, const QString& eyebrow, const QString& title,
                   QWidget* trailing = nullptr) {
    auto* row = new QHBoxLayout;
    row->setSpacing(12);
    auto* textColumn = new QVBoxLayout;
    textColumn->setSpacing(4);
    textColumn->addWidget(monoCaption(eyebrow));
    textColumn->addWidget(titleLabel(title));
    row->addLayout(textColumn, 1);
    if (trailing) row->addWidget(trailing, 0, Qt::AlignTop);
    layout->addLayout(row);
}

QWidget* page(const QString& eyebrow, const QString& title, QVBoxLayout*& layout) {
    auto* scroll = makePageColumn(layout);
    addPageHeader(layout, eyebrow, title);
    return scroll;
}

QTableWidget* table(const QStringList& headers) {
    auto* widget = new QTableWidget;
    widget->setColumnCount(headers.size());
    widget->setHorizontalHeaderLabels(headers);
    widget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    widget->verticalHeader()->hide();
    widget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    widget->setSelectionBehavior(QAbstractItemView::SelectRows);
    widget->setSelectionMode(QAbstractItemView::SingleSelection);
    widget->setAlternatingRowColors(true);
    widget->setShowGrid(false);
    widget->setFocusPolicy(Qt::NoFocus);
    widget->verticalHeader()->setDefaultSectionSize(42);
    widget->horizontalHeader()->setMinimumHeight(44);
    return widget;
}

std::int64_t localDayStart(int daysAgo = 0) {
    return QDateTime(QDate::currentDate().addDays(-daysAgo), QTime(0, 0))
        .toMSecsSinceEpoch();
}

AnalyticsConfig localAnalyticsConfig() {
    AnalyticsConfig config;
    config.timezone_offset_minutes =
        QDateTime::currentDateTime().offsetFromUtc() / 60;
    return config;
}

QString insightPrefix(Insight::Kind kind) {
    switch (kind) {
    case Insight::Kind::Positive: return "Positive";
    case Insight::Kind::Warning: return "Attention";
    case Insight::Kind::Recommendation: return "Try";
    case Insight::Kind::Neutral: return "Note";
    }
    return "Note";
}

QColor categoryBarColor(const QString& category, bool distraction) {
    if (distraction || category == "Entertainment") return QColor("#EF4444");
    if (category == "Coding") return QColor("#00DFA2");
    if (category == "Communication") return QColor("#F59E0B");
    if (category == "Research" || category == "Work") return QColor("#7B8CDE");
    if (category == "Planning" || category == "Administration") return QColor("#A78BFA");
    return QColor("#7B8CDE");
}

QString sessionTimelineType(const Session& session) {
    if (session.distraction || session.category == "Entertainment") return "distraction";
    if (session.category == "Communication") return "comms";
    if (session.productive &&
        session.active_duration_ms >= 30 * kMinuteMs) return "deep";
    if (session.productive) return "shallow";
    return "idle";
}

double hourOfDay(qint64 unixMs) {
    const auto time = QDateTime::fromMSecsSinceEpoch(unixMs).time();
    return time.hour() + time.minute() / 60.0 + time.second() / 3600.0;
}

QString clockTime(qint64 unixMs) {
    return QDateTime::fromMSecsSinceEpoch(unixMs).toString("HH:mm");
}

int sessionDurationMs(const Session& session) {
    if (session.end_unix_ms <= session.start_unix_ms) return 0;
    const auto elapsed = session.end_unix_ms - session.start_unix_ms;
    if (session.active_duration_ms <= 0) return static_cast<int>(elapsed);
    return static_cast<int>(std::min(session.active_duration_ms, elapsed));
}

int sessionScore(const Session& session) {
    int score = 68;
    const auto minutes = session.active_duration_ms / kMinuteMs;
    if (minutes >= 120) score += 22;
    else if (minutes >= 90) score += 16;
    else if (minutes >= 60) score += 10;
    else if (minutes >= 30) score += 6;
    if (session.productive && !session.distraction) score += 8;
    if (session.distraction) score -= 18;
    return std::clamp(score, 0, 100);
}

QWidget* legendRow() {
    struct LegendItem { QString type; QString label; QString color; };
    const std::array<LegendItem, 5> items{{
        {"deep", "Deep work", "#00DFA2"},
        {"shallow", "Shallow work", "#7B8CDE"},
        {"comms", "Comms", "#F59E0B"},
        {"distraction", "Distraction", "#EF4444"},
        {"idle", "Idle / break", "#2A2D38"},
    }};
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    for (const auto& item : items) {
        auto* chip = new QHBoxLayout;
        chip->setSpacing(6);
        auto* swatch = new QLabel;
        swatch->setFixedSize(10, 10);
        swatch->setStyleSheet(
            QString("background:%1;border-radius:2px;").arg(item.color));
        chip->addWidget(swatch);
        chip->addWidget(monoCaption(item.label));
        layout->addLayout(chip);
    }
    layout->addStretch();
    return row;
}

QWidget* hourLabels(double startHour, double endHour) {
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    for (int hour = static_cast<int>(startHour); hour <= static_cast<int>(endHour);
         hour += 2) {
        auto* label = monoCaption(QString("%1:00").arg(hour));
        if (hour == static_cast<int>(endHour)) {
            layout->addWidget(label, 0, Qt::AlignRight);
        } else if (hour == static_cast<int>(startHour)) {
            layout->addWidget(label, 0, Qt::AlignLeft);
        } else {
            layout->addWidget(label, 0, Qt::AlignHCenter);
        }
    }
    return row;
}

bool setLaunchAtLogin(bool enabled) {
#if defined(Q_OS_WIN)
    QSettings startup(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        QSettings::NativeFormat);
    if (enabled) {
        startup.setValue("ActivityOS",
                         QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
    } else {
        startup.remove("ActivityOS");
    }
    return startup.status() == QSettings::NoError;
#elif defined(Q_OS_MACOS)
    const auto directory = QDir::homePath() + "/Library/LaunchAgents";
    QDir().mkpath(directory);
    const auto path = directory + "/com.activityos.desktop.plist";
    if (!enabled) return QFile::remove(path) || !QFile::exists(path);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
    QTextStream stream(&file);
    stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
              "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
           << "<plist version=\"1.0\"><dict>"
           << "<key>Label</key><string>com.activityos.desktop</string>"
           << "<key>ProgramArguments</key><array><string>"
           << QCoreApplication::applicationFilePath().toHtmlEscaped()
           << "</string></array><key>RunAtLoad</key><true/>"
           << "</dict></plist>\n";
    return stream.status() == QTextStream::Ok;
#else
    const auto directory = QDir::homePath() + "/.config/autostart";
    QDir().mkpath(directory);
    const auto path = directory + "/activityos.desktop";
    if (!enabled) return QFile::remove(path) || !QFile::exists(path);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
    QTextStream stream(&file);
    stream << "[Desktop Entry]\nType=Application\nName=ActivityOS\nExec=\""
           << QCoreApplication::applicationFilePath().replace("\"", "\\\"")
           << "\"\nX-GNOME-Autostart-enabled=true\n";
    return stream.status() == QTextStream::Ok;
#endif
}

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
        #appBreakdownName { font-size: 14px; font-weight: 500; color: @TEXT; }
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
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
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

} // namespace

MainWindow::MainWindow(storage::Database& database, QWidget* parent)
    : QMainWindow(parent),
      database_(database),
      activity_(database, localAnalyticsConfig()),
      tracker_(std::make_unique<TrackerService>(database, makePlatformActivitySource())) {
    setWindowTitle("ActivityOS");
    resize(1200, 780);
    setMinimumSize(960, 640);
    QFont appFont = QApplication::font();
    appFont.setStyleStrategy(QFont::PreferAntialias);
    QApplication::setFont(appFont);
    qApp->installEventFilter(this);
    buildShell();
    buildTray();

    trackerTimer_ = new QTimer(this);
    trackerTimer_->setInterval(5000);
    connect(trackerTimer_, &QTimer::timeout, this, [this] { refreshTracker(); });
    trackerTimer_->start();

    refreshTimer_ = new QTimer(this);
    refreshTimer_->setInterval(30'000);
    connect(refreshTimer_, &QTimer::timeout, this, [this] { refresh(); });
    refreshTimer_->start();
    refreshTracker();
    refresh();
}

MainWindow::~MainWindow() {
    qApp->removeEventFilter(this);
    if (tracker_) tracker_->shutdown(unixMillisecondsNow());
}

void MainWindow::buildShell() {
    auto* central = new QWidget;
    auto* shell = new QHBoxLayout(central);
    shell->setContentsMargins(0, 0, 0, 0);
    shell->setSpacing(0);

    auto* sidebar = new QFrame;
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(224);
    auto* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);

    auto* sidebarHeader = new QFrame;
    sidebarHeader->setObjectName("sidebarHeader");
    auto* headerLayout = new QHBoxLayout(sidebarHeader);
    headerLayout->setContentsMargins(20, 20, 20, 20);
    headerLayout->setSpacing(10);
    auto* logoBadge = new QLabel("A");
    logoBadge->setObjectName("logoBadge");
    auto* brand = new QLabel("ActivityOS");
    brand->setObjectName("brand");
    headerLayout->addWidget(logoBadge, 0, Qt::AlignVCenter);
    headerLayout->addWidget(brand, 0, Qt::AlignVCenter);
    headerLayout->addStretch();
    sidebarLayout->addWidget(sidebarHeader);

    auto* navContainer = new QWidget;
    auto* navLayout = new QVBoxLayout(navContainer);
    navLayout->setContentsMargins(12, 12, 12, 12);
    navigation_ = new QListWidget;
    navigation_->setObjectName("navigation");
    navigation_->addItems({"◈  Today", "⊙  Focus", "⊞  Apps", "⟋  Trends", "◎  Goals",
                           "⊕  Settings"});
    navigation_->setCurrentRow(0);
    navLayout->addWidget(navigation_, 1);
    sidebarLayout->addWidget(navContainer, 1);

    auto* trayFrame = new QFrame;
    trayFrame->setObjectName("sidebarTrayFrame");
    auto* trayFrameLayout = new QVBoxLayout(trayFrame);
    trayFrameLayout->setContentsMargins(16, 16, 16, 16);
    auto* trayBox = new QFrame;
    trayBox->setObjectName("trayStatusBox");
    auto* trayLayout = new QVBoxLayout(trayBox);
    trayLayout->setContentsMargins(12, 12, 12, 12);
    trayLayout->setSpacing(8);
    auto* trayStatusRow = new QHBoxLayout;
    trayStatusRow->setSpacing(8);
    auto* trayDot = new QLabel;
    trayDot->setObjectName("recordingDot");
    trayDot->setFixedSize(6, 6);
    sidebarTrayStatus_ = new QLabel("Starting…");
    sidebarTrayStatus_->setObjectName("trayStatusLabel");
    trayStatusRow->addWidget(trayDot);
    trayStatusRow->addWidget(sidebarTrayStatus_);
    trayStatusRow->addStretch();
    sidebarTrayDetail_ = new QLabel("Waiting for activity");
    sidebarTrayDetail_->setObjectName("trayStatusDetail");
    sidebarTrayDetail_->setWordWrap(true);
    trayLayout->addLayout(trayStatusRow);
    trayLayout->addWidget(sidebarTrayDetail_);
    trayFrameLayout->addWidget(trayBox);
    sidebarLayout->addWidget(trayFrame);

    pages_ = new QStackedWidget;
    pages_->setObjectName("pages");
    pages_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    pages_->addWidget(buildTodayPage());
    pages_->addWidget(buildFocusPage());
    pages_->addWidget(buildAppsPage());
    pages_->addWidget(buildTrendsPage());
    pages_->addWidget(buildGoalsPage());
    pages_->addWidget(buildSettingsPage());
    connect(navigation_, &QListWidget::currentRowChanged, pages_,
            &QStackedWidget::setCurrentIndex);

    shell->addWidget(sidebar);
    shell->addWidget(pages_, 1);
    setCentralWidget(central);
    qApp->setStyleSheet(friendlyStyleSheet(qApp->palette()));
    statusBar()->showMessage("Local-only · everything stays on this device");
}

QWidget* MainWindow::buildTodayPage() {
    QVBoxLayout* layout;
    auto* container = makePageColumn(layout);

    auto* recording = makeRecordingIndicator(trackingState_);
    addPageHeader(layout, QDate::currentDate().toString("dddd, d MMM yyyy"),
                  "Today's Activity", recording);

    auto* cards = new QGridLayout;
    cards->setHorizontalSpacing(12);
    cards->setVerticalSpacing(12);
    cards->addWidget(metricCard("Active Time", activeValue_, activeSub_), 0, 0);
    cards->addWidget(metricCard("Deep Focus", deepWorkValue_, deepWorkSub_, true), 0, 1);
    cards->addWidget(metricCard("Distractions", distractionValue_, distractionSub_), 0, 2);
    cards->addWidget(metricCard("Focus Score", scoreValue_, scoreSub_, true), 0, 3);
    equalizeColumns(cards);
    layout->addLayout(cards);

    auto* timelineBody = new QWidget;
    auto* timelineLayout = new QVBoxLayout(timelineBody);
    timelineLayout->setContentsMargins(0, 0, 0, 0);
    timelineLayout->setSpacing(10);
    timelineCaption_ = monoCaption("Day Timeline  ·  08:00 – 18:00");
    timelineLayout->addWidget(timelineCaption_);
    dayTimeline_ = new DayTimelineWidget;
    dayTimeline_->setRange(8, 18);
    timelineLayout->addWidget(dayTimeline_);
    timelineLayout->addWidget(hourLabels(8, 18));
    timelineLayout->addWidget(legendRow());
    layout->addWidget(wrapSection("", timelineBody));

    appBreakdown_ = new AppBreakdownWidget;
    layout->addWidget(wrapSection("Application Breakdown", appBreakdown_));

    auto* bottom = new QGridLayout;
    bottom->setHorizontalSpacing(12);
    bottom->addWidget(metricCard("Context Switches", switchesValue_, switchesSub_), 0, 0);
    bottom->addWidget(metricCard("Avg Session Length", avgSessionValue_, avgSessionSub_, true),
                       0, 1);
    bottom->addWidget(metricCard("Recovery Time", recoveryValue_, recoverySub_), 0, 2);
    equalizeColumns(bottom);
    layout->addLayout(bottom);

    layout->addStretch();
    return container;
}

QWidget* MainWindow::buildFocusPage() {
    QVBoxLayout* layout;
    auto* container = page("Deep Work", "Focus Sessions", layout);

    auto* cards = new QGridLayout;
    cards->setHorizontalSpacing(12);
    cards->addWidget(metricCard("Total Focus", focusTotalValue_, focusTotalSub_, true), 0, 0);
    cards->addWidget(metricCard("Longest Session", focusLongestValue_, focusLongestSub_), 0, 1);
    cards->addWidget(metricCard("Avg Score", focusAvgScoreValue_, focusAvgScoreSub_), 0, 2);
    cards->addWidget(metricCard("Flow Entries", focusFlowValue_, focusFlowSub_, true), 0, 3);
    equalizeColumns(cards);
    layout->addLayout(cards);

    focusSessionsTable_ = table({"Start", "End", "Duration", "Primary App", "Score"});
    layout->addWidget(wrapSection("Session Log", focusSessionsTable_), 1);
    return container;
}

QWidget* MainWindow::buildAppsPage() {
    QVBoxLayout* layout;
    auto* container = page("Usage", "Applications", layout);

    auto* help = new QLabel(
        "Chrome tabs are classified from the active site URL when macOS allows ActivityOS "
        "to control Google Chrome. Other browsers fall back to visible titles.");
    help->setObjectName("activeApp");
    help->setWordWrap(true);
    layout->addWidget(help);

    auto* addRule = new QPushButton("Add Classification Rule");
    auto* removeRule = new QPushButton("Delete Selected Rule");
    connect(addRule, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const auto app = QInputDialog::getText(this, "Classification Rule",
                                               "Application name contains:",
                                               QLineEdit::Normal, {}, &ok);
        if (!ok || app.trimmed().isEmpty()) return;
        const auto title = QInputDialog::getText(
            this, "Classification Rule",
            "Optional window-title text (useful for browser tabs):",
            QLineEdit::Normal, {}, &ok);
        if (!ok) return;
        const QStringList categories{"Coding", "Research", "Communication", "Planning",
                                     "Administration", "Work", "Entertainment", "General",
                                     "Other"};
        const auto category = QInputDialog::getItem(this, "Classification Rule", "Category:",
                                                    categories, 0, true, &ok);
        if (!ok) return;
        const bool distraction =
            QMessageBox::question(this, "Classification Rule",
                                  "Should this rule count as a potential distraction?",
                                  QMessageBox::Yes | QMessageBox::No,
                                  category == "Entertainment" ? QMessageBox::Yes
                                                              : QMessageBox::No) ==
            QMessageBox::Yes;
        storage::ClassificationRule rule;
        rule.application_pattern = app.toStdString();
        rule.title_pattern = title.toStdString();
        rule.category = category.toStdString();
        rule.is_distraction = distraction;
        rule.priority = 100;
        rule.created_at = rule.updated_at = unixMillisecondsNow();
        activity_.saveRule(rule);
        refreshApps();
    });
    connect(removeRule, &QPushButton::clicked, this, [this] {
        const int row = rulesTable_->currentRow();
        const auto rules = activity_.rules();
        if (row >= 0 && static_cast<std::size_t>(row) < rules.size()) {
            activity_.deleteRule(rules[static_cast<std::size_t>(row)].id);
            refreshApps();
        }
    });
    auto* actions = new QHBoxLayout;
    actions->addWidget(addRule);
    actions->addWidget(removeRule);
    actions->addStretch();
    layout->addLayout(actions);

    appsTable_ = table({"Application", "Category", "Time Today", "Share", "Status"});
    layout->addWidget(wrapSection("Application Usage", appsTable_), 1);
    rulesTable_ = table({"Application Pattern", "Title Pattern", "Category", "Distraction",
                         "Priority"});
    layout->addWidget(wrapSection("Classification Rules", rulesTable_), 1);
    return container;
}

QWidget* MainWindow::buildTrendsPage() {
    QVBoxLayout* layout;
    auto* container = page("Analytics", "Weekly Trends", layout);

    auto* cards = new QGridLayout;
    cards->setHorizontalSpacing(12);
    cards->addWidget(metricCard("Avg Focus/Day", trendsAvgFocusValue_, trendsAvgFocusSub_, true),
                    0, 0);
    cards->addWidget(metricCard("Best Day", trendsBestDayValue_, trendsBestDaySub_), 0, 1);
    cards->addWidget(metricCard("Focus Score", trendsScoreValue_, trendsScoreSub_), 0, 2);
    cards->addWidget(metricCard("Streak", trendsStreakValue_, trendsStreakSub_, true), 0, 3);
    equalizeColumns(cards);
    layout->addLayout(cards);

    weeklyChart_ = new WeeklyChartWidget;
    layout->addWidget(wrapSection("Daily Breakdown  ·  Last 7 Days", weeklyChart_));

    auto* profileGrid = new QGridLayout;
    profileGrid->setHorizontalSpacing(32);
    profileGrid->setVerticalSpacing(16);
    workstylePeak_ = new QLabel("—");
    workstyleType_ = new QLabel("—");
    workstyleDayLength_ = new QLabel("—");
    workstyleDeepRatio_ = new QLabel("—");
    const auto addProfileCell = [&](int row, int col, const QString& label, QLabel*& value) {
        auto* cell = new QVBoxLayout;
        cell->setSpacing(4);
        cell->addWidget(monoCaption(label));
        value->setObjectName("profileValue");
        cell->addWidget(value);
        profileGrid->addLayout(cell, row, col);
    };
    addProfileCell(0, 0, "Peak hours", workstylePeak_);
    addProfileCell(0, 1, "Work type", workstyleType_);
    addProfileCell(1, 0, "Avg day length", workstyleDayLength_);
    addProfileCell(1, 1, "Deep work ratio", workstyleDeepRatio_);
    auto* profileWidget = new QWidget;
    profileWidget->setLayout(profileGrid);
    layout->addWidget(wrapSection("Workstyle Profile  ·  14-day Baseline", profileWidget));

    historyTable_ = table({"Date", "Active", "Focus", "Deep Work", "Distraction", "Switches"});
    layout->addWidget(wrapSection("Daily History", historyTable_), 1);
    return container;
}

QWidget* MainWindow::buildGoalsPage() {
    QVBoxLayout* layout;
    auto* container = page("Productivity", "Goals & Experiments", layout);

    auto* add = new QPushButton("New Goal");
    auto* remove = new QPushButton("Delete Selected");
    connect(add, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const auto name = QInputDialog::getText(this, "New Goal", "Goal name:",
                                                QLineEdit::Normal, {}, &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        const QStringList metrics{"focus_time", "deep_work", "distraction_time",
                                  "context_switches", "switch_rate", "active_time"};
        const auto metric =
            QInputDialog::getItem(this, "New Goal", "Metric:", metrics, 0, false, &ok);
        if (!ok) return;
        const double target = QInputDialog::getDouble(
            this, "New Goal",
            metric.contains("time") || metric == "deep_work"
                ? "Target minutes:"
                : "Target value:",
            60, 0.01, 100000, 1, &ok);
        if (!ok) return;
        storage::Goal goal;
        goal.name = name.toStdString();
        goal.metric = metric.toStdString();
        goal.target_value =
            metric.contains("time") || metric == "deep_work" ? target * kMinuteMs : target;
        goal.period = "weekly";
        goal.created_at = unixMillisecondsNow();
        activity_.saveGoal(goal);
        refreshGoals();
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        const int row = goalsTable_->currentRow();
        const auto goals = database_.goals(false);
        if (row >= 0 && static_cast<std::size_t>(row) < goals.size()) {
            activity_.deleteGoal(goals[static_cast<std::size_t>(row)].id);
            refreshGoals();
        }
    });
    auto* actions = new QHBoxLayout;
    actions->addWidget(add);
    actions->addWidget(remove);
    actions->addStretch();
    layout->addLayout(actions);

    goalsCardsLayout_ = new QVBoxLayout;
    goalsCardsLayout_->setSpacing(12);
    auto* goalsHost = new QWidget;
    goalsHost->setLayout(goalsCardsLayout_);
    layout->addWidget(goalsHost);

    experimentCard_ = new QFrame;
    experimentCard_->setObjectName("sectionCard");
    auto* experimentLayout = new QVBoxLayout(experimentCard_);
    experimentLayout->setContentsMargins(20, 20, 20, 20);
    experimentLayout->setSpacing(12);
    experimentLayout->addWidget(monoCaption("Active Experiment"));
    experimentTitle_ = new QLabel("No active experiment");
    experimentTitle_->setObjectName("goalTitle");
    experimentTitle_->setWordWrap(true);
    experimentBody_ = new QLabel;
    experimentBody_->setObjectName("activeApp");
    experimentBody_->setWordWrap(true);
    experimentLayout->addWidget(experimentTitle_);
    experimentLayout->addWidget(experimentBody_);
    auto* experimentStats = new QGridLayout;
    experimentStats->setHorizontalSpacing(16);
    QLabel* unusedBefore = nullptr;
    QLabel* unusedAfter = nullptr;
    QLabel* unusedDelta = nullptr;
    experimentStats->addWidget(metricCard("Before", experimentBefore_, unusedBefore), 0, 0);
    experimentStats->addWidget(metricCard("After", experimentAfter_, unusedAfter, true), 0, 1);
    experimentStats->addWidget(metricCard("Change", experimentDelta_, unusedDelta, true), 0, 2);
    unusedBefore->setText("baseline");
    unusedAfter->setText("latest");
    unusedDelta->setText("vs baseline");
    equalizeColumns(experimentStats);
    experimentLayout->addLayout(experimentStats);
    layout->addWidget(experimentCard_);

    goalsTable_ = table({"Goal", "Metric", "Current", "Target", "Progress", "Status"});
    layout->addWidget(wrapSection("All Goals", goalsTable_), 1);
    experimentsTable_ = table({"Experiment", "Hypothesis", "Intervention", "Status", "Result"});
    experimentsTable_->hide();
    return container;
}

QWidget* MainWindow::buildSettingsPage() {
    QVBoxLayout* layout;
    auto* container = page("Configuration", "Settings", layout);

    auto* settingsCard = new QFrame;
    settingsCard->setObjectName("sectionCard");
    auto* settingsLayout = new QVBoxLayout(settingsCard);
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    settingsLayout->setSpacing(0);

    settingsLayout->addWidget(settingsSection("Privacy"));
    pauseTracking_ = new QCheckBox;
    storeTitles_ = new QCheckBox;
    launchAtLogin_ = new QCheckBox;
    settingsLayout->addWidget(settingsRow("Pause tracking",
                                          "Stop recording activity until you resume.",
                                          pauseTracking_));
    settingsLayout->addWidget(settingsRow("Record window titles",
                                          "Include window and tab titles in session data.",
                                          storeTitles_));
    settingsLayout->addWidget(settingsRow("Launch at login",
                                          "Start ActivityOS when you log in.",
                                          launchAtLogin_));

    settingsLayout->addWidget(settingsSection("Data"));
    idleThreshold_ = new QSpinBox;
    idleThreshold_->setRange(1, 60);
    idleThreshold_->setSuffix(" min");
    idleThreshold_->setMinimumWidth(92);
    idleThreshold_->setValue(5);
    settingsLayout->addWidget(settingsRow("Auto-pause on idle",
                                          "Stop recording after this many minutes of inactivity.",
                                          idleThreshold_));
    layout->addWidget(settingsCard);

    auto* exportCsv = new QPushButton("Export CSV");
    auto* exportJson = new QPushButton("Export JSON");
    auto* deleteAll = new QPushButton("Delete All Data");
    deleteAll->setObjectName("dangerButton");
    auto* demoData = new QPushButton("Load Safe Demo Data");
    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(10);
    buttons->addWidget(deleteAll);
    buttons->addWidget(exportCsv);
    buttons->addWidget(exportJson);
    buttons->addStretch();
    buttons->addWidget(demoData);
    layout->addLayout(buttons);
    layout->addStretch();

    connect(exportCsv, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getSaveFileName(this, "Export Activity", "activity.csv",
                                                       "CSV files (*.csv)");
        if (!path.isEmpty()) activity_.exportCsv({0, unixMillisecondsNow() + 1},
                                                 path.toStdString());
    });
    connect(exportJson, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getSaveFileName(this, "Export Activity", "activity.json",
                                                       "JSON files (*.json)");
        if (!path.isEmpty()) activity_.exportJson({0, unixMillisecondsNow() + 1},
                                                  path.toStdString());
    });
    connect(deleteAll, &QPushButton::clicked, this, [this] {
        if (QMessageBox::warning(this, "Delete All Activity",
                                 "This permanently deletes all recorded activity. Continue?",
                                 QMessageBox::Yes | QMessageBox::Cancel,
                                 QMessageBox::Cancel) == QMessageBox::Yes) {
            activity_.deleteAllActivity();
            refresh();
        }
    });

    connect(pauseTracking_, &QCheckBox::toggled, this, [this](bool paused) {
        if (paused) tracker_->pause(unixMillisecondsNow());
        else tracker_->resume();
        database_.setSetting("tracking_paused", paused ? "1" : "0");
        refreshTracker();
    });
    const auto updateTrackerConfig = [this] {
        TrackerConfig config;
        config.idle_threshold_ms =
            static_cast<std::int64_t>(idleThreshold_->value()) * kMinuteMs;
        config.persist_window_titles = storeTitles_->isChecked();
        tracker_->updateConfig(config);
        database_.setSetting("persist_window_titles", storeTitles_->isChecked() ? "1" : "0");
        database_.setSetting("idle_threshold_minutes",
                             std::to_string(idleThreshold_->value()));
    };
    connect(storeTitles_, &QCheckBox::toggled, this,
            [updateTrackerConfig](bool) { updateTrackerConfig(); });
    connect(idleThreshold_, &QSpinBox::valueChanged, this,
            [updateTrackerConfig](int) { updateTrackerConfig(); });
    connect(launchAtLogin_, &QCheckBox::toggled, this, [this](bool enabled) {
        if (setLaunchAtLogin(enabled)) {
            database_.setSetting("launch_at_login", enabled ? "1" : "0");
        } else {
            QMessageBox::warning(this, "Launch at Login",
                                 "ActivityOS could not update your login settings.");
        }
    });
    connect(demoData, &QPushButton::clicked, this, [this] {
        loadDemoData();
        refresh();
    });

    storeTitles_->setChecked(database_.setting("persist_window_titles") == "1");
    pauseTracking_->setChecked(database_.setting("tracking_paused") == "1");
    launchAtLogin_->setChecked(database_.setting("launch_at_login") == "1");
    if (const auto value = database_.setting("idle_threshold_minutes")) {
        idleThreshold_->setValue(std::max(1, std::stoi(*value)));
    }
    updateTrackerConfig();
    return container;
}

void MainWindow::buildTray() {
    auto icon = QApplication::windowIcon();
    if (icon.isNull()) {
        QPixmap pixmap(32, 32);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QColor("#00DFA2"));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(2, 2, 28, 28, 6, 6);
        painter.setPen(QColor("#060809"));
        QFont font = painter.font();
        font.setBold(true);
        font.setPointSize(14);
        painter.setFont(font);
        painter.drawText(pixmap.rect(), Qt::AlignCenter, "A");
        icon = QIcon(pixmap);
    }
    tray_ = new QSystemTrayIcon(icon, this);
    tray_->setToolTip("ActivityOS");
    auto* menu = new QMenu(this);
    auto* open = menu->addAction("Open ActivityOS");
    auto* pause = menu->addAction("Pause Tracking");
    pause->setCheckable(true);
    menu->addSeparator();
    auto* quit = menu->addAction("Quit");
    connect(open, &QAction::triggered, this, [this] {
        show();
        raise();
        activateWindow();
    });
    connect(pause, &QAction::toggled, this, [this](bool value) {
        pauseTracking_->setChecked(value);
    });
    connect(quit, &QAction::triggered, this, [this] {
        tracker_->shutdown(unixMillisecondsNow());
        QApplication::quit();
    });
    connect(tray_, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger) {
                    show();
                    raise();
                    activateWindow();
                }
            });
    tray_->setContextMenu(menu);
    tray_->show();
}

storage::DateRange MainWindow::todayRange() const {
    const auto start = localDayStart();
    return {start, start + kDayMs};
}

void MainWindow::refreshTracker() {
    const auto status = tracker_->poll(unixMillisecondsNow());
    if (status.paused) {
        trackingState_->setObjectName("trackingPill");
        trackingState_->setText("PAUSED");
        sidebarTrayStatus_->setText("Paused");
    } else if (status.idle) {
        trackingState_->setObjectName("trackingPill");
        trackingState_->setText("IDLE");
        sidebarTrayStatus_->setText("Idle");
    } else if (status.source_status == ActivitySourceStatus::unsupported) {
        trackingState_->setObjectName("trackingPill");
        trackingState_->setText("UNAVAILABLE");
        sidebarTrayStatus_->setText("Unavailable");
    } else {
        trackingState_->setObjectName("recordingLabel");
        trackingState_->setText("RECORDING");
        sidebarTrayStatus_->setText("Active");
    }
    trackingState_->style()->unpolish(trackingState_);
    trackingState_->style()->polish(trackingState_);

    sidebarTrayDetail_->setText(
        status.active_application.empty()
            ? "Waiting for activity"
            : QString("%1 · %2")
                  .arg(QString::fromStdString(status.active_application),
                       QString::fromStdString(status.active_category)));
}

void MainWindow::refresh() {
    try {
        const auto range = todayRange();
        const auto snapshot = activity_.dashboard(range, range.start);
        refreshToday(snapshot);
        refreshFocus(snapshot);
        refreshApps();
        refreshTrends(snapshot);
        refreshGoals();
        refreshExperimentCard();
        statusBar()->showMessage("Updated " + QTime::currentTime().toString("h:mm:ss AP"));
    } catch (const std::exception& error) {
        statusBar()->showMessage("Update failed: " + QString::fromUtf8(error.what()));
    }
}

void MainWindow::refreshToday(const DashboardSnapshot& snapshot) {
    activeValue_->setText(duration(snapshot.metrics.active_ms));
    if (snapshot.baseline.active_ms > 0) {
        const auto delta = snapshot.metrics.active_ms - snapshot.baseline.active_ms;
        activeSub_->setText(delta >= 0
                                ? QString("↑ %1 vs typical day").arg(duration(delta))
                                : QString("↓ %1 vs typical day").arg(duration(-delta)));
    } else {
        activeSub_->setText("tracked today");
    }

    deepWorkValue_->setText(duration(snapshot.metrics.deep_work_ms));
    deepWorkSub_->setText(QString("%1 sessions").arg(snapshot.metrics.focus_session_count));

    distractionValue_->setText(duration(snapshot.metrics.distraction_ms));
    if (snapshot.baseline.distraction_ms > 0) {
        const auto delta = snapshot.metrics.distraction_ms - snapshot.baseline.distraction_ms;
        const auto pct = std::abs(delta) * 100.0 / snapshot.baseline.distraction_ms;
        distractionSub_->setText(delta <= 0 ? QString("↓ %1% vs baseline").arg(int(pct))
                                            : QString("↑ %1% vs baseline").arg(int(pct)));
    } else {
        distractionSub_->setText("building baseline");
    }

    scoreValue_->setText(QString("%1/100").arg(snapshot.score.score));
    scoreSub_->setText("productivity score");

    switchesValue_->setText(QString::number(snapshot.metrics.context_switch_count));
    switchesSub_->setText("disruptive switches");

    avgSessionValue_->setText(duration(snapshot.metrics.average_session_ms));
    if (snapshot.baseline.average_session_ms > 0) {
        const auto delta = snapshot.metrics.average_session_ms - snapshot.baseline.average_session_ms;
        avgSessionSub_->setText(delta >= 0 ? QString("↑ %1 vs baseline").arg(duration(delta))
                                          : QString("↓ %1 vs baseline").arg(duration(-delta)));
    } else {
        avgSessionSub_->setText("building baseline");
    }

    recoveryValue_->setText(snapshot.metrics.average_recovery_ms > 0
                                ? duration(snapshot.metrics.average_recovery_ms)
                                : "—");
    recoverySub_->setText("avg after distraction");

    const auto daySessions = activity_.sessions(todayRange());
    std::vector<TimelineBlock> blocks;
    blocks.reserve(daySessions.size());
    double timelineStart = 8.0;
    double timelineEnd = 18.0;
    if (!daySessions.empty()) {
        timelineStart = std::floor(hourOfDay(daySessions.front().start_unix_ms));
        timelineEnd = std::ceil(hourOfDay(daySessions.back().end_unix_ms));
        timelineStart = std::min(timelineStart, 8.0);
        timelineEnd = std::max(timelineEnd, 18.0);
    }
    for (const auto& session : daySessions) {
        TimelineBlock block;
        block.startHour = hourOfDay(session.start_unix_ms);
        block.endHour = hourOfDay(session.end_unix_ms);
        block.type = sessionTimelineType(session);
        block.label = QString::fromStdString(session.category);
        blocks.push_back(std::move(block));
    }
    dayTimeline_->setRange(timelineStart, timelineEnd);
    dayTimeline_->setBlocks(std::move(blocks));
    timelineCaption_->setText(QString("Day Timeline  ·  %1:00 – %2:00")
                                  .arg(int(timelineStart))
                                  .arg(int(timelineEnd)));

    std::unordered_map<std::string, std::int64_t> appMinutes;
    for (const auto& session : daySessions) {
        appMinutes[session.app] += session.active_duration_ms / kMinuteMs;
    }
    std::vector<std::pair<std::string, std::int64_t>> ranked(appMinutes.begin(), appMinutes.end());
    std::sort(ranked.begin(), ranked.end(),
              [](const auto& left, const auto& right) { return left.second > right.second; });
    const auto topTotal = ranked.empty()
                              ? 0
                              : std::accumulate(ranked.begin(), ranked.end(), std::int64_t{0},
                                                [](std::int64_t sum, const auto& item) {
                                                    return sum + item.second;
                                                });
    std::vector<AppUsageRow> rows;
    for (const auto& [name, minutes] : ranked) {
        if (rows.size() >= 8) break;
        const auto category = std::find_if(daySessions.begin(), daySessions.end(),
                                           [&](const Session& session) {
                                               return session.app == name;
                                           });
        const QString categoryName =
            category != daySessions.end() ? QString::fromStdString(category->category) : "Other";
        AppUsageRow row;
        row.name = QString::fromStdString(name);
        row.minutes = static_cast<int>(minutes);
        row.pct = topTotal > 0 ? static_cast<int>((minutes * 100) / topTotal) : 0;
        row.color = categoryBarColor(categoryName, category != daySessions.end() && category->distraction);
        rows.push_back(row);
    }
    appBreakdown_->setRows(std::move(rows));
}

void MainWindow::refreshFocus(const DashboardSnapshot& snapshot) {
    focusTotalValue_->setText(duration(snapshot.metrics.focused_ms));
    focusTotalSub_->setText("today");
    focusLongestValue_->setText(duration(snapshot.metrics.longest_session_ms));
    focusLongestSub_->setText("longest block");

    const auto daySessions = activity_.sessions(todayRange());
    const auto switches = activity_.switches(todayRange());
    AnalyticsEngine engine(localAnalyticsConfig());
    const auto focusBlocks = engine.focusSessions(daySessions, switches);
    int totalScore = 0;
    int flowEntries = 0;
    for (const auto& block : focusBlocks) {
        const auto score = sessionScore(block);
        totalScore += score;
        if (score >= 85) ++flowEntries;
    }
    focusAvgScoreValue_->setText(focusBlocks.empty()
                                     ? "—"
                                     : QString("%1/100").arg(totalScore /
                                                             static_cast<int>(focusBlocks.size())));
    focusAvgScoreSub_->setText(QString("across %1 sessions").arg(focusBlocks.size()));
    focusFlowValue_->setText(QString::number(flowEntries));
    focusFlowSub_->setText("score ≥ 85");

    focusSessionsTable_->setRowCount(static_cast<int>(focusBlocks.size()));
    for (int row = 0; row < focusSessionsTable_->rowCount(); ++row) {
        const auto& block = focusBlocks[static_cast<std::size_t>(row)];
        const std::array<QString, 5> values{
            clockTime(block.start_unix_ms), clockTime(block.end_unix_ms),
            duration(sessionDurationMs(block)), QString::fromStdString(block.app),
            QString::number(sessionScore(block))};
        for (int column = 0; column < static_cast<int>(values.size()); ++column) {
            focusSessionsTable_->setItem(row, column, new QTableWidgetItem(values[column]));
        }
    }
}

void MainWindow::refreshApps() {
    const auto range = todayRange();
    const auto daySessions = activity_.sessions(range);
    std::unordered_map<std::string, std::int64_t> appMinutes;
    std::unordered_map<std::string, Session> appSample;
    for (const auto& session : daySessions) {
        appMinutes[session.app] += session.active_duration_ms;
        appSample[session.app] = session;
    }
    const auto totalActive = std::accumulate(
        appMinutes.begin(), appMinutes.end(), std::int64_t{0},
        [](std::int64_t sum, const auto& item) { return sum + item.second; });

    std::vector<std::pair<std::string, std::int64_t>> ranked(appMinutes.begin(), appMinutes.end());
    std::sort(ranked.begin(), ranked.end(),
              [](const auto& left, const auto& right) { return left.second > right.second; });

    appsTable_->setRowCount(static_cast<int>(ranked.size()));
    for (int row = 0; row < appsTable_->rowCount(); ++row) {
        const auto& [name, ms] = ranked[static_cast<std::size_t>(row)];
        const auto& sample = appSample.at(name);
        const auto minutes = ms / kMinuteMs;
        const auto share = totalActive > 0 ? (ms * 100 / totalActive) : 0;
        const QString status =
            sample.distraction ? "distraction"
                               : (sample.productive ? "productive" : "neutral");
        const std::array<QString, 5> values{
            QString::fromStdString(name), QString::fromStdString(sample.category),
            QString("%1m").arg(minutes), QString("%1%").arg(share), status};
        for (int column = 0; column < static_cast<int>(values.size()); ++column) {
            appsTable_->setItem(row, column, new QTableWidgetItem(values[column]));
        }
    }

    const auto rules = activity_.rules();
    rulesTable_->setRowCount(static_cast<int>(rules.size()));
    for (int row = 0; row < rulesTable_->rowCount(); ++row) {
        const auto& rule = rules[static_cast<std::size_t>(row)];
        const std::array<QString, 5> values{
            QString::fromStdString(rule.application_pattern),
            QString::fromStdString(rule.title_pattern), QString::fromStdString(rule.category),
            rule.is_distraction ? "Yes" : "No", QString::number(rule.priority)};
        for (int column = 0; column < static_cast<int>(values.size()); ++column) {
            rulesTable_->setItem(row, column, new QTableWidgetItem(values[column]));
        }
    }
}

void MainWindow::refreshTrends(const DashboardSnapshot& snapshot) {
    const storage::DateRange weekRange{localDayStart(6), localDayStart() + kDayMs};
    const auto days = activity_.dailyHistory(weekRange);
    double totalFocus = 0;
    int bestIndex = -1;
    std::int64_t bestFocus = 0;
    for (int index = 0; index < static_cast<int>(days.size()); ++index) {
        totalFocus += days[static_cast<std::size_t>(index)].focused_ms;
        if (days[static_cast<std::size_t>(index)].focused_ms >= bestFocus) {
            bestFocus = days[static_cast<std::size_t>(index)].focused_ms;
            bestIndex = index;
        }
    }
    trendsAvgFocusValue_->setText(days.empty()
                                      ? "—"
                                      : duration(static_cast<std::int64_t>(totalFocus / days.size())));
    trendsAvgFocusSub_->setText("7-day average");
    trendsBestDayValue_->setText(bestIndex >= 0
                                     ? QDateTime::fromMSecsSinceEpoch(
                                           days[static_cast<std::size_t>(bestIndex)].day_start_unix_ms)
                                           .date()
                                           .toString("ddd")
                                     : "—");
    trendsBestDaySub_->setText(bestIndex >= 0 ? duration(bestFocus) : "—");
    trendsScoreValue_->setText(QString("%1/100").arg(snapshot.score.score));
    trendsScoreSub_->setText("today's score");
    trendsStreakValue_->setText("—");
    trendsStreakSub_->setText("focus goal tracking");

    std::vector<WeekDayBars> chartDays;
    chartDays.reserve(days.size());
    for (const auto& day : days) {
        WeekDayBars bars;
        const auto date = QDateTime::fromMSecsSinceEpoch(day.day_start_unix_ms).date();
        bars.day = date == QDate::currentDate() ? "Today" : date.toString("ddd");
        bars.isToday = date == QDate::currentDate();
        bars.segments = {
            {day.idle_ms / static_cast<double>(kHourMs), QColor("#2A2D38")},
            {day.distraction_ms / static_cast<double>(kHourMs), QColor("#F59E0B")},
            {(day.active_ms - day.focused_ms - day.distraction_ms) / static_cast<double>(kHourMs),
             QColor("#7B8CDE")},
            {day.focused_ms / static_cast<double>(kHourMs), QColor("#00DFA2")},
        };
        chartDays.push_back(std::move(bars));
    }
    weeklyChart_->setDays(std::move(chartDays));

    const auto& profile = snapshot.profile;
    workstylePeak_->setText(profile.peak_half_hour < 0
                                ? "Not enough data"
                                : QString("%1:%2 – %3:%4")
                                      .arg(profile.peak_half_hour / 2, 2, 10, QChar('0'))
                                      .arg((profile.peak_half_hour % 2) * 30, 2, 10, QChar('0'))
                                      .arg(profile.peak_half_hour / 2 + 1, 2, 10, QChar('0'))
                                      .arg((profile.peak_half_hour % 2) * 30, 2, 10, QChar('0')));
    workstyleType_->setText(QString::fromStdString(profile.focus_pattern));
    workstyleDayLength_->setText(duration(snapshot.metrics.workday_elapsed_ms));
    const auto deepRatio =
        snapshot.metrics.active_ms > 0
            ? snapshot.metrics.deep_work_ms * 100 / snapshot.metrics.active_ms
            : 0;
    workstyleDeepRatio_->setText(QString("%1%").arg(deepRatio));

    const storage::DateRange historyRange{localDayStart(13), localDayStart() + kDayMs};
    const auto history = activity_.dailyHistory(historyRange);
    historyTable_->setRowCount(static_cast<int>(history.size()));
    for (int row = 0; row < historyTable_->rowCount(); ++row) {
        const auto& day = history[static_cast<std::size_t>(row)];
        const auto date = QDateTime::fromMSecsSinceEpoch(day.day_start_unix_ms).date();
        const std::array<QString, 6> values{
            date.toString("ddd, MMM d"), duration(day.active_ms), duration(day.focused_ms),
            duration(day.deep_work_ms), duration(day.distraction_ms),
            QString::number(day.context_switch_count)};
        for (int column = 0; column < static_cast<int>(values.size()); ++column) {
            historyTable_->setItem(row, column, new QTableWidgetItem(values[column]));
        }
    }
}

void MainWindow::refreshGoals() {
    while (auto* item = goalsCardsLayout_->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    const storage::DateRange range{localDayStart(6), localDayStart() + kDayMs};
    const auto goals = activity_.goalProgress(range);
    goalsTable_->setRowCount(static_cast<int>(goals.size()));
    for (int row = 0; row < goalsTable_->rowCount(); ++row) {
        const auto& goal = goals[static_cast<std::size_t>(row)];
        const bool timeMetric = goal.goal.metric.find("time") != std::string::npos ||
                                goal.goal.metric == "deep_work";
        const std::array<QString, 6> values{
            QString::fromStdString(goal.goal.name), QString::fromStdString(goal.goal.metric),
            timeMetric ? duration(static_cast<std::int64_t>(goal.current_value))
                       : QString::number(goal.current_value, 'f', 1),
            timeMetric ? duration(static_cast<std::int64_t>(goal.goal.target_value))
                       : QString::number(goal.goal.target_value, 'f', 1),
            percent(goal.progress), goal.achieved ? "achieved" : "on-track"};
        for (int column = 0; column < static_cast<int>(values.size()); ++column) {
            goalsTable_->setItem(row, column, new QTableWidgetItem(values[column]));
        }

        auto* card = new QFrame;
        card->setObjectName("sectionCard");
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(20, 18, 20, 18);
        cardLayout->setSpacing(10);
        auto* titleRow = new QHBoxLayout;
        auto* name = new QLabel(QString::fromStdString(goal.goal.name));
        name->setObjectName("goalTitle");
        name->setWordWrap(true);
        auto* badge = new QLabel(values.back());
        badge->setObjectName(goal.achieved ? "statusBadge" : "statusBadgeTrack");
        titleRow->addWidget(name, 1);
        titleRow->addWidget(badge, 0, Qt::AlignTop);
        cardLayout->addLayout(titleRow);
        auto* detail = new QLabel(QString("%1 of %2").arg(values[2], values[3]));
        detail->setObjectName("activeApp");
        cardLayout->addWidget(detail);
        auto* progress = new QProgressBar;
        progress->setRange(0, 100);
        progress->setValue(std::clamp(static_cast<int>(goal.progress * 100.0), 0, 100));
        progress->setTextVisible(false);
        cardLayout->addWidget(progress);
        auto* percentLabel = new QLabel(percent(goal.progress));
        percentLabel->setObjectName("metricSub");
        cardLayout->addWidget(percentLabel);
        goalsCardsLayout_->addWidget(card);
    }
    if (goals.empty()) {
        goalsCardsLayout_->addWidget(new QLabel("No goals yet. Add one to start tracking progress."));
    }
}

void MainWindow::refreshExperimentCard() {
    const auto experiments = activity_.experimentResults();
    const auto active = std::find_if(experiments.begin(), experiments.end(), [](const auto& item) {
        return item.experiment.status == "active";
    });
    if (active == experiments.end()) {
        experimentTitle_->setText("No active experiment");
        experimentBody_->setText("Create an experiment to compare a baseline with an intervention.");
        experimentBefore_->setText("—");
        experimentAfter_->setText("—");
        experimentDelta_->setText("—");
        return;
    }
    experimentTitle_->setText(QString::fromStdString(active->experiment.name));
    experimentBody_->setText(QString::fromStdString(active->experiment.intervention));
    experimentBefore_->setText(active->experiment.baseline_value
                                   ? QString::number(*active->experiment.baseline_value, 'f', 0)
                                   : "—");
    experimentAfter_->setText(active->experiment.result_value
                                  ? QString::number(*active->experiment.result_value, 'f', 0)
                                  : "—");
    experimentDelta_->setText(
        (active->change_percent >= 0 ? "+" : "") +
        QString::number(active->change_percent, 'f', 0));
}

void MainWindow::loadDemoData() {
    if (database_.setting("demo_data_version") == "1") {
        QMessageBox::information(this, "Demo Data", "Demo data is already loaded.");
        return;
    }
    if (QMessageBox::question(
            this, "Load Demo Data",
            "Add two weeks of clearly labeled synthetic activity to preview every dashboard?") !=
        QMessageBox::Yes) {
        return;
    }

    struct DemoApp {
        const char* name;
        const char* category;
        bool distraction;
        std::int64_t id{};
    };
    std::array<DemoApp, 5> apps{{
        {"Demo IDE", "Coding", false},
        {"Demo Browser Docs", "Research", false},
        {"Demo Team Chat", "Communication", false},
        {"Demo Video", "Entertainment", true},
        {"Demo Planner", "Planning", false},
    }};
    const auto now = unixMillisecondsNow();
    for (auto& app : apps) {
        storage::Application value;
        value.name = app.name;
        value.category = app.category;
        value.is_distraction = app.distraction;
        value.created_at = now;
        app.id = database_.upsertApplication(value);
    }

    storage::Transaction transaction(database_);
    for (int daysAgo = 13; daysAgo >= 0; --daysAgo) {
        const auto day = localDayStart(daysAgo);
        const auto variation = static_cast<std::int64_t>((13 - daysAgo) % 4) * 8 * kMinuteMs;
        struct Block {
            int app;
            std::int64_t start;
            std::int64_t length;
        };
        const std::array<Block, 7> blocks{{
            {4, 8 * kHourMs + 45 * kMinuteMs, 20 * kMinuteMs},
            {0, 9 * kHourMs + 10 * kMinuteMs, 92 * kMinuteMs + variation},
            {1, 11 * kHourMs, 38 * kMinuteMs},
            {2, 11 * kHourMs + 42 * kMinuteMs, 12 * kMinuteMs},
            {0, 13 * kHourMs, 70 * kMinuteMs + variation / 2},
            {3, 14 * kHourMs + 18 * kMinuteMs, (5 + daysAgo % 3) * kMinuteMs},
            {0, 14 * kHourMs + 30 * kMinuteMs, 48 * kMinuteMs},
        }};
        std::optional<std::int64_t> previous;
        for (const auto& block : blocks) {
            storage::Session session;
            session.application_id = apps[block.app].id;
            session.category = apps[block.app].category;
            session.start_time = day + block.start;
            session.end_time = session.start_time + block.length;
            session.duration_ms = block.length;
            database_.addSession(session);
            database_.addActivityEvent(
                {0, apps[block.app].id, session.start_time, "DEMO", ""});
            if (previous) {
                database_.addContextSwitch(
                    {0, previous, apps[block.app].id, session.start_time});
            }
            previous = apps[block.app].id;
        }
        database_.addIdlePeriod(
            {0, day + 12 * kHourMs, day + 13 * kHourMs, kHourMs});
    }

    storage::Goal goal;
    goal.name = "15 focused hours this week";
    goal.metric = "focus_time";
    goal.target_value = 15 * kHourMs;
    goal.period = "weekly";
    goal.created_at = now;
    database_.saveGoal(goal);

    storage::Experiment experiment;
    experiment.name = "Quiet mornings";
    experiment.hypothesis = "Muting communication before 11 AM may increase focused work.";
    experiment.intervention = "Mute demo team chat from 9–11 AM for seven days.";
    experiment.start_time = now - 7 * kDayMs;
    experiment.end_time = now;
    experiment.status = "completed";
    experiment.baseline_value = 134.0;
    experiment.result_value = 171.0;
    experiment.notes = "Synthetic demonstration only";
    experiment.created_at = now - 14 * kDayMs;
    database_.saveExperiment(experiment);
    database_.setSetting("demo_data_version", "1");
    transaction.commit();
    QMessageBox::information(this, "Demo Data Loaded",
                             "Two weeks of synthetic activity are ready to explore.");
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    if (dayTimeline_) dayTimeline_->update();
    if (weeklyChart_) weeklyChart_->update();
    if (pages_) pages_->updateGeometry();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    hide();
    tray_->showMessage("ActivityOS is still tracking",
                       "Use the tray menu to pause tracking or quit.",
                       QSystemTrayIcon::Information, 2500);
    event->ignore();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == qApp && event->type() == QEvent::ApplicationActivate &&
        !isVisible()) {
        showNormal();
        raise();
        activateWindow();
        return true;
    }
    return QMainWindow::eventFilter(watched, event);
}

} // namespace activityos::ui
