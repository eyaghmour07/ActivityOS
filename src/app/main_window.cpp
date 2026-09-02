#include "main_window.hpp"

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
#include <QGraphicsDropShadowEffect>
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
#include <cmath>
#include <filesystem>

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

QLabel* titleLabel(const QString& text) {
    auto* label = new QLabel(text);
    label->setObjectName("pageTitle");
    QFont font = label->font();
    font.setPointSize(24);
    font.setBold(true);
    label->setFont(font);
    return label;
}

QFrame* metricCard(const QString& label, QLabel*& value) {
    auto* frame = new QFrame;
    frame->setObjectName("metricCard");
    frame->setMinimumHeight(104);
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(18, 15, 18, 15);
    layout->setSpacing(7);
    auto* caption = new QLabel(label);
    caption->setObjectName("metricCaption");
    value = new QLabel("—");
    value->setObjectName("metricValue");
    layout->addWidget(caption);
    layout->addWidget(value);
    auto* shadow = new QGraphicsDropShadowEffect(frame);
    shadow->setBlurRadius(18);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 28));
    frame->setGraphicsEffect(shadow);
    return frame;
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
    widget->verticalHeader()->setDefaultSectionSize(38);
    widget->horizontalHeader()->setMinimumHeight(40);
    return widget;
}

QWidget* page(const QString& title, QVBoxLayout*& layout) {
    auto* container = new QWidget;
    layout = new QVBoxLayout(container);
    layout->setContentsMargins(32, 28, 32, 28);
    layout->setSpacing(18);
    layout->addWidget(titleLabel(title));
    return container;
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
    const bool dark = palette.color(QPalette::Window).lightness() < 128;
    const QString background = dark ? "#111318" : "#f5f7fb";
    const QString surface = dark ? "#191c23" : "#ffffff";
    const QString surfaceAlt = dark ? "#20242d" : "#eef2f7";
    const QString border = dark ? "#343a48" : "#dbe1ea";
    const QString text = dark ? "#f4f6fb" : "#172033";
    const QString muted = dark ? "#aeb7c7" : "#667085";
    const QString accent = dark ? "#8aa4ff" : "#4f6ef7";
    const QString accentSoft = dark ? "#252d43" : "#e7ecff";
    const QString selection = dark ? "#303b59" : "#dce4ff";

    QString style = R"(
        * { color: @TEXT; }
        QMainWindow, QWidget, QDialog { background: @BG; color: @TEXT; }
        #sidebar { background: @SURFACE; border-right: 1px solid @BORDER; }
        #brand { font-size: 23px; font-weight: 700; color: @ACCENT; padding-left: 8px; }
        #tagline { font-size: 12px; color: @MUTED; padding-left: 8px; }
        #localBadge {
            background: @SURFACE_ALT;
            border: 1px solid @BORDER;
            border-radius: 9px;
            padding: 9px;
            font-size: 12px;
            color: @MUTED;
        }
        #navigation { border: 0; background: transparent; outline: 0; }
        #navigation::item {
            color: @TEXT;
            min-height: 25px;
            padding: 9px 13px;
            margin: 2px 0;
            border-radius: 8px;
        }
        #navigation::item:hover { background: @SURFACE_ALT; color: @TEXT; }
        #navigation::item:selected {
            background: @ACCENT_SOFT;
            color: @TEXT;
            border-left: 3px solid @ACCENT;
            font-weight: 650;
        }
        #pageTitle { color: @TEXT; }
        #trackingPill {
            color: @TEXT;
            background: @SURFACE_ALT;
            border: 1px solid @BORDER;
            border-radius: 10px;
            padding: 7px 11px;
            font-weight: 650;
        }
        #activeApp { color: @MUTED; font-size: 13px; }
        #metricCard {
            color: @TEXT;
            background: @SURFACE;
            border: 1px solid @BORDER;
            border-radius: 12px;
        }
        #metricCard:hover { border: 1px solid @ACCENT; }
        #metricCaption { color: @MUTED; font-size: 12px; font-weight: 600; }
        #metricValue { color: @TEXT; font-size: 25px; font-weight: 700; }
        QPushButton {
            color: @TEXT;
            background: @SURFACE_ALT;
            border: 1px solid @BORDER;
            border-radius: 8px;
            padding: 8px 14px;
            font-weight: 600;
        }
        QPushButton:hover { color: @TEXT; border-color: @ACCENT; background: @ACCENT_SOFT; }
        QPushButton:pressed { color: @TEXT; background: @SELECTION; }
        QPushButton:disabled { color: @MUTED; border-color: @BORDER; }
        QTableWidget, QListWidget {
            color: @TEXT;
            background: @SURFACE;
            alternate-background-color: @SURFACE_ALT;
            border: 1px solid @BORDER;
            border-radius: 10px;
            padding: 2px;
            outline: 0;
        }
        QTableWidget::item, QListWidget::item { color: @TEXT; }
        QTableWidget::item:selected, QListWidget::item:selected {
            color: @TEXT;
            background: @SELECTION;
        }
        #insightList { padding: 6px; }
        #insightList::item {
            color: @TEXT;
            background: @SURFACE_ALT;
            border-radius: 8px;
            margin: 4px;
            padding: 12px;
        }
        #insightList::item:selected { color: @TEXT; background: @SELECTION; }
        QHeaderView::section {
            color: @TEXT;
            background: @SURFACE_ALT;
            border: 0;
            border-bottom: 1px solid @BORDER;
            padding: 8px;
            font-weight: 650;
        }
        QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            color: @TEXT;
            background: @SURFACE;
            selection-color: @TEXT;
            selection-background-color: @SELECTION;
            border: 1px solid @BORDER;
            border-radius: 7px;
            padding: 7px;
        }
        QComboBox QAbstractItemView {
            color: @TEXT;
            background: @SURFACE;
            selection-color: @TEXT;
            selection-background-color: @SELECTION;
            border: 1px solid @BORDER;
        }
        QCheckBox { color: @TEXT; spacing: 8px; }
        QMenu {
            color: @TEXT;
            background: @SURFACE;
            border: 1px solid @BORDER;
            padding: 5px;
        }
        QMenu::item { color: @TEXT; padding: 7px 22px; border-radius: 5px; }
        QMenu::item:selected { color: @TEXT; background: @SELECTION; }
        QToolTip {
            color: @TEXT;
            background: @SURFACE_ALT;
            border: 1px solid @BORDER;
            padding: 5px;
        }
        QMessageBox QLabel { color: @TEXT; background: transparent; }
        QStatusBar { color: @MUTED; background: @SURFACE; border-top: 1px solid @BORDER; }
        QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
        QScrollBar::handle:vertical {
            background: @BORDER;
            min-height: 28px;
            border-radius: 4px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )";
    style.replace("@ACCENT_SOFT", accentSoft);
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
    resize(1180, 760);
    setMinimumSize(940, 620);
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
    sidebar->setFixedWidth(226);
    auto* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(14, 22, 14, 18);
    sidebarLayout->setSpacing(10);
    auto* brand = new QLabel("ActivityOS");
    brand->setObjectName("brand");
    auto* tagline = new QLabel("Your work, made visible");
    tagline->setObjectName("tagline");
    sidebarLayout->addWidget(brand);
    sidebarLayout->addWidget(tagline);
    sidebarLayout->addSpacing(18);

    navigation_ = new QListWidget;
    navigation_->setObjectName("navigation");
    navigation_->addItems({"Overview", "History", "Analytics", "Weekly report", "Goals",
                           "Experiments", "Insights", "App rules", "Privacy", "Settings"});
    navigation_->setCurrentRow(0);
    sidebarLayout->addWidget(navigation_, 1);
    auto* localBadge = new QLabel("  Local-only · Private");
    localBadge->setObjectName("localBadge");
    localBadge->setToolTip("Your activity stays in a local SQLite database.");
    sidebarLayout->addWidget(localBadge);

    pages_ = new QStackedWidget;
    pages_->addWidget(buildTodayPage());
    pages_->addWidget(buildHistoryPage());
    pages_->addWidget(buildAnalyticsPage());
    pages_->addWidget(buildWeeklyPage());
    pages_->addWidget(buildGoalsPage());
    pages_->addWidget(buildExperimentsPage());
    pages_->addWidget(buildInsightsPage());
    pages_->addWidget(buildRulesPage());
    pages_->addWidget(buildPrivacyPage());
    pages_->addWidget(buildSettingsPage());
    connect(navigation_, &QListWidget::currentRowChanged, pages_,
            &QStackedWidget::setCurrentIndex);

    shell->addWidget(sidebar);
    shell->addWidget(pages_, 1);
    setCentralWidget(central);
    qApp->setStyleSheet(friendlyStyleSheet(qApp->palette()));
    statusBar()->showMessage("Everything stays on this device");
}

QWidget* MainWindow::buildTodayPage() {
    QVBoxLayout* layout;
    auto* container = page("Good to see you", layout);
    auto* subtitle = new QLabel(
        "A private snapshot of how your workday is taking shape.");
    subtitle->setObjectName("activeApp");
    layout->addWidget(subtitle);

    auto* statusRow = new QHBoxLayout;
    trackingState_ = new QLabel("Starting tracker…");
    trackingState_->setObjectName("trackingPill");
    activeApplication_ = new QLabel;
    activeApplication_->setObjectName("activeApp");
    auto* refreshButton = new QPushButton("Refresh");
    connect(refreshButton, &QPushButton::clicked, this, [this] { refresh(); });
    statusRow->addWidget(trackingState_, 0);
    statusRow->addWidget(activeApplication_, 1);
    statusRow->addWidget(refreshButton, 0);
    layout->addLayout(statusRow);

    auto* cards = new QGridLayout;
    cards->setHorizontalSpacing(14);
    cards->setVerticalSpacing(14);
    cards->addWidget(metricCard("FOCUSED WORK", focusedValue_), 0, 0);
    cards->addWidget(metricCard("DEEP WORK", deepWorkValue_), 0, 1);
    cards->addWidget(metricCard("DISTRACTION", distractionValue_), 0, 2);
    cards->addWidget(metricCard("CONTEXT SWITCHES", switchesValue_), 1, 0);
    cards->addWidget(metricCard("PRODUCTIVITY SCORE", scoreValue_), 1, 1);
    cards->addWidget(metricCard("WORKDAY SPAN", workdayValue_), 1, 2);
    layout->addLayout(cards);

    auto* columns = new QHBoxLayout;
    auto* distributionColumn = new QVBoxLayout;
    auto* distributionLabel = new QLabel("Where your time went");
    distributionLabel->setStyleSheet("font-size: 15px; font-weight: 650;");
    categoryTable_ = table({"Category", "Time", "Share"});
    distributionColumn->addWidget(distributionLabel);
    distributionColumn->addWidget(categoryTable_);
    auto* insightColumn = new QVBoxLayout;
    auto* insightLabel = new QLabel("What stands out");
    insightLabel->setStyleSheet("font-size: 15px; font-weight: 650;");
    todayInsights_ = new QListWidget;
    todayInsights_->setObjectName("insightList");
    todayInsights_->setWordWrap(true);
    insightColumn->addWidget(insightLabel);
    insightColumn->addWidget(todayInsights_);
    columns->addLayout(distributionColumn, 3);
    columns->addLayout(insightColumn, 2);
    layout->addLayout(columns, 1);
    return container;
}

QWidget* MainWindow::buildHistoryPage() {
    QVBoxLayout* layout;
    auto* container = page("History", layout);
    layout->addWidget(new QLabel("Your last 14 days, compared only with your own activity."));
    historyTable_ = table({"Date", "Active", "Focus", "Deep Work", "Distraction", "Switches"});
    layout->addWidget(historyTable_, 1);
    return container;
}

QWidget* MainWindow::buildAnalyticsPage() {
    QVBoxLayout* layout;
    auto* container = page("Workstyle Analytics", layout);
    profileText_ = new QLabel;
    profileText_->setWordWrap(true);
    layout->addWidget(profileText_);
    auto* columns = new QHBoxLayout;
    transitionsTable_ = table({"Common Transition", "Count"});
    distractionsTable_ = table({"App", "Duration", "Recovery", "Cost", "Severity"});
    columns->addWidget(transitionsTable_, 2);
    columns->addWidget(distractionsTable_, 3);
    layout->addLayout(columns, 1);
    return container;
}

QWidget* MainWindow::buildWeeklyPage() {
    QVBoxLayout* layout;
    auto* container = page("Weekly Report", layout);
    weeklyTable_ = table({"Week", "Active", "Focus", "Deep Work", "Distraction", "Switches"});
    layout->addWidget(weeklyTable_, 1);
    return container;
}

QWidget* MainWindow::buildGoalsPage() {
    QVBoxLayout* layout;
    auto* container = page("Goals", layout);
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
    goalsTable_ = table({"Goal", "Metric", "Current", "Target", "Progress", "Status"});
    layout->addWidget(goalsTable_, 1);
    return container;
}

QWidget* MainWindow::buildExperimentsPage() {
    QVBoxLayout* layout;
    auto* container = page("Productivity Experiments", layout);
    layout->addWidget(new QLabel(
        "Compare a baseline with an intervention. Results show association, not causation."));
    auto* add = new QPushButton("New Experiment");
    auto* remove = new QPushButton("Delete Selected");
    connect(add, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const auto name = QInputDialog::getText(this, "New Experiment", "Name:",
                                                QLineEdit::Normal, {}, &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        const auto hypothesis = QInputDialog::getMultiLineText(
            this, "New Experiment", "Hypothesis:", {}, &ok);
        if (!ok) return;
        const auto intervention = QInputDialog::getMultiLineText(
            this, "New Experiment", "Intervention:", {}, &ok);
        if (!ok) return;
        storage::Experiment experiment;
        experiment.name = name.toStdString();
        experiment.hypothesis = hypothesis.toStdString();
        experiment.intervention = intervention.toStdString();
        experiment.start_time = unixMillisecondsNow();
        experiment.status = "active";
        experiment.created_at = unixMillisecondsNow();
        activity_.saveExperiment(experiment);
        refreshExperiments();
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        const int row = experimentsTable_->currentRow();
        const auto experiments = database_.experiments();
        if (row >= 0 && static_cast<std::size_t>(row) < experiments.size()) {
            activity_.deleteExperiment(experiments[static_cast<std::size_t>(row)].id);
            refreshExperiments();
        }
    });
    auto* actions = new QHBoxLayout;
    actions->addWidget(add);
    actions->addWidget(remove);
    actions->addStretch();
    layout->addLayout(actions);
    experimentsTable_ =
        table({"Experiment", "Hypothesis", "Intervention", "Status", "Result"});
    layout->addWidget(experimentsTable_, 1);
    return container;
}

QWidget* MainWindow::buildInsightsPage() {
    QVBoxLayout* layout;
    auto* container = page("Explainable Insights", layout);
    layout->addWidget(new QLabel(
        "Every insight states what changed, the personal baseline, and why it matters."));
    insightsList_ = new QListWidget;
    insightsList_->setWordWrap(true);
    layout->addWidget(insightsList_, 1);
    return container;
}

QWidget* MainWindow::buildRulesPage() {
    QVBoxLayout* layout;
    auto* container = page("Application Rules", layout);
    auto* browserHelp = new QLabel(
        "Browser tabs are classified from their visible title without storing the title. "
        "Homework, school platforms, and browser document editors count as Work; "
        "developer and AI-assistant sites count as Research; common streaming sites "
        "count as Entertainment. Add a rule below whenever your use differs.");
    browserHelp->setObjectName("activeApp");
    browserHelp->setWordWrap(true);
    layout->addWidget(browserHelp);
    auto* add = new QPushButton("Add Classification Rule");
    auto* remove = new QPushButton("Delete Selected");
    connect(add, &QPushButton::clicked, this, [this] {
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
        refreshRules();
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        const int row = rulesTable_->currentRow();
        const auto rules = activity_.rules();
        if (row >= 0 && static_cast<std::size_t>(row) < rules.size()) {
            activity_.deleteRule(rules[static_cast<std::size_t>(row)].id);
            refreshRules();
        }
    });
    auto* actions = new QHBoxLayout;
    actions->addWidget(add);
    actions->addWidget(remove);
    actions->addStretch();
    layout->addLayout(actions);
    rulesTable_ = table({"Application Pattern", "Title Pattern", "Category", "Distraction",
                         "Priority"});
    layout->addWidget(rulesTable_, 1);
    return container;
}

QWidget* MainWindow::buildPrivacyPage() {
    QVBoxLayout* layout;
    auto* container = page("Privacy & Data", layout);
    layout->addWidget(new QLabel(
        "ActivityOS is local-only. It never records keystrokes, screenshots, webcam data, "
        "or uploads your activity."));

    auto* exportCsv = new QPushButton("Export CSV…");
    auto* exportJson = new QPushButton("Export JSON…");
    auto* deleteToday = new QPushButton("Delete Today’s Activity");
    auto* deleteAll = new QPushButton("Delete All Activity");
    auto* excludeApp = new QPushButton("Exclude Application…");
    auto* excludeCategory = new QPushButton("Exclude Category…");
    auto* restoreExclusion = new QPushButton("Restore Exclusion…");
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(exportCsv);
    buttons->addWidget(exportJson);
    buttons->addStretch();
    layout->addLayout(buttons);
    auto* exclusions = new QHBoxLayout;
    exclusions->addWidget(excludeApp);
    exclusions->addWidget(excludeCategory);
    exclusions->addWidget(restoreExclusion);
    exclusions->addStretch();
    layout->addLayout(exclusions);
    layout->addWidget(deleteToday);
    layout->addWidget(deleteAll);

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
    connect(deleteToday, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, "Delete Today",
                                  "Permanently delete today’s recorded activity?") ==
            QMessageBox::Yes) {
            activity_.deleteRange(todayRange());
            refresh();
        }
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
    connect(excludeApp, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const auto name = QInputDialog::getText(this, "Exclude Application",
                                                "Application name:", QLineEdit::Normal,
                                                {}, &ok);
        if (ok && !name.trimmed().isEmpty()) {
            database_.excludeApplication(name.toStdString());
            QMessageBox::information(this, "Application Excluded",
                                     "Future activity from this application will not be stored.");
        }
    });
    connect(excludeCategory, &QPushButton::clicked, this, [this] {
        const QStringList categories{"Coding", "Research", "Communication", "Planning",
                                     "Administration", "Work", "Entertainment", "General",
                                     "Other"};
        bool ok = false;
        const auto category = QInputDialog::getItem(
            this, "Exclude Category", "Category:", categories, 0, true, &ok);
        if (ok) {
            database_.excludeCategory(category.toStdString());
            QMessageBox::information(this, "Category Excluded",
                                     "Future activity in this category will not be stored.");
        }
    });
    connect(restoreExclusion, &QPushButton::clicked, this, [this] {
        QStringList choices;
        for (const auto& app : database_.excludedApplications()) {
            choices << "Application: " + QString::fromStdString(app);
        }
        for (const auto& category : database_.excludedCategories()) {
            choices << "Category: " + QString::fromStdString(category);
        }
        if (choices.empty()) {
            QMessageBox::information(this, "Exclusions", "There are no active exclusions.");
            return;
        }
        bool ok = false;
        const auto choice = QInputDialog::getItem(
            this, "Restore Exclusion", "Allow tracking again:", choices, 0, false, &ok);
        if (!ok) return;
        if (choice.startsWith("Application: ")) {
            database_.includeApplication(choice.mid(13).toStdString());
        } else if (choice.startsWith("Category: ")) {
            database_.includeCategory(choice.mid(10).toStdString());
        }
    });
    applicationsTable_ = table({"Application", "Category", "Distraction"});
    layout->addWidget(applicationsTable_, 1);
    return container;
}

QWidget* MainWindow::buildSettingsPage() {
    QVBoxLayout* layout;
    auto* container = page("Settings", layout);
    auto* form = new QFormLayout;
    pauseTracking_ = new QCheckBox("Pause all activity tracking");
    storeTitles_ = new QCheckBox("Store window titles (off by default)");
    idleThreshold_ = new QSpinBox;
    idleThreshold_->setRange(1, 60);
    idleThreshold_->setSuffix(" minutes");
    idleThreshold_->setValue(5);
    auto* launchAtLogin = new QCheckBox("Launch ActivityOS at login");
    auto* demoData = new QPushButton("Load Safe Demo Data");
    form->addRow(pauseTracking_);
    form->addRow(storeTitles_);
    form->addRow("Idle threshold:", idleThreshold_);
    form->addRow(launchAtLogin);
    layout->addLayout(form);
    layout->addWidget(demoData, 0, Qt::AlignLeft);
    layout->addStretch();

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
    connect(launchAtLogin, &QCheckBox::toggled, this, [this](bool enabled) {
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
    launchAtLogin->setChecked(database_.setting("launch_at_login") == "1");
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
        painter.setBrush(QColor("#4f7cff"));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(2, 2, 28, 28, 7, 7);
        painter.setPen(Qt::white);
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
        trackingState_->setText("Tracking paused");
    } else if (status.idle) {
        trackingState_->setText("Idle");
    } else if (status.source_status == ActivitySourceStatus::unsupported) {
        trackingState_->setText("Tracking unavailable on this desktop session");
    } else {
        trackingState_->setText("Tracking locally");
    }
    activeApplication_->setText(
        status.active_application.empty()
            ? status.message ? QString::fromStdString(*status.message) : QString()
            : QString("Active: %1 · %2")
                  .arg(QString::fromStdString(status.active_application),
                       QString::fromStdString(status.active_category)));
}

void MainWindow::refresh() {
    try {
        const auto range = todayRange();
        const auto snapshot = activity_.dashboard(range, range.start);
        refreshToday(snapshot);
        refreshHistory();
        refreshAnalytics(snapshot);
        refreshWeekly();
        refreshGoals();
        refreshExperiments();
        refreshRules();
        refreshApplications();
        statusBar()->showMessage("Updated " + QTime::currentTime().toString("h:mm:ss AP"));
    } catch (const std::exception& error) {
        statusBar()->showMessage("Update failed: " + QString::fromUtf8(error.what()));
    }
}

void MainWindow::refreshToday(const DashboardSnapshot& snapshot) {
    focusedValue_->setText(duration(snapshot.metrics.focused_ms));
    deepWorkValue_->setText(duration(snapshot.metrics.deep_work_ms));
    distractionValue_->setText(duration(snapshot.metrics.distraction_ms));
    switchesValue_->setText(QString::number(snapshot.metrics.context_switch_count));
    scoreValue_->setText(QString::number(snapshot.score.score) + "/100");
    workdayValue_->setText(duration(snapshot.metrics.workday_elapsed_ms));

    categoryTable_->setRowCount(static_cast<int>(snapshot.metrics.categories.size()));
    for (int row = 0; row < categoryTable_->rowCount(); ++row) {
        const auto& category = snapshot.metrics.categories[static_cast<std::size_t>(row)];
        categoryTable_->setItem(row, 0,
                                new QTableWidgetItem(QString::fromStdString(category.category)));
        categoryTable_->setItem(row, 1, new QTableWidgetItem(duration(category.active_ms)));
        categoryTable_->setItem(
            row, 2,
            new QTableWidgetItem(QString::number(category.percentage, 'f', 0) + "%"));
    }

    todayInsights_->clear();
    insightsList_->clear();
    for (const auto& insight : snapshot.insights) {
        const auto text = QString("%1: %2\n%3")
                              .arg(insightPrefix(insight.kind),
                                   QString::fromStdString(insight.title),
                                   QString::fromStdString(insight.explanation));
        todayInsights_->addItem(text);
        insightsList_->addItem(text);
    }
    for (const auto& trigger : snapshot.potential_triggers) {
        const auto text =
            QString("Potential trigger: %1 occurred %2 times. This pattern is associative, "
                    "not evidence of causation.")
                .arg(QString::fromStdString(trigger.first))
                .arg(trigger.second);
        todayInsights_->addItem(text);
        insightsList_->addItem(text);
    }
    if (snapshot.insights.empty()) {
        todayInsights_->addItem("Keep tracking to establish your personal 14-day baseline.");
        insightsList_->addItem("More activity is needed before ActivityOS can identify patterns.");
    }
}

void MainWindow::refreshHistory() {
    const storage::DateRange range{localDayStart(13), localDayStart() + kDayMs};
    const auto days = activity_.dailyHistory(range);
    historyTable_->setRowCount(static_cast<int>(days.size()));
    for (int row = 0; row < historyTable_->rowCount(); ++row) {
        const auto& day = days[static_cast<std::size_t>(row)];
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

void MainWindow::refreshAnalytics(const DashboardSnapshot& snapshot) {
    const auto& profile = snapshot.profile;
    profileText_->setText(
        QString("Focus pattern: %1  ·  Context switching: %2  ·  Distraction sensitivity: "
                "%3\nTypical sustained session: %4  ·  Recovery: %5  ·  Peak half-hour: %6")
            .arg(QString::fromStdString(profile.focus_pattern),
                 QString::fromStdString(profile.context_switching),
                 QString::fromStdString(profile.distraction_sensitivity),
                 duration(profile.average_sustained_session_ms),
                 duration(profile.average_recovery_ms),
                 profile.peak_half_hour < 0
                     ? "Not enough data"
                     : QString("%1:%2")
                           .arg(profile.peak_half_hour / 2, 2, 10, QChar('0'))
                           .arg((profile.peak_half_hour % 2) * 30, 2, 10, QChar('0'))));

    transitionsTable_->setRowCount(static_cast<int>(snapshot.transitions.size()));
    for (int row = 0; row < transitionsTable_->rowCount(); ++row) {
        const auto& transition = snapshot.transitions[static_cast<std::size_t>(row)];
        transitionsTable_->setItem(row, 0,
                                   new QTableWidgetItem(QString::fromStdString(transition.first)));
        transitionsTable_->setItem(row, 1,
                                   new QTableWidgetItem(QString::number(transition.second)));
    }
    distractionsTable_->setRowCount(static_cast<int>(snapshot.distractions.size()));
    for (int row = 0; row < distractionsTable_->rowCount(); ++row) {
        const auto& episode = snapshot.distractions[static_cast<std::size_t>(row)];
        QString severity = "Minor";
        if (episode.severity == DistractionEpisode::Severity::Moderate) severity = "Moderate";
        if (episode.severity == DistractionEpisode::Severity::Major) severity = "Major";
        const std::array<QString, 5> values{
            QString::fromStdString(episode.app), duration(episode.duration_ms),
            duration(episode.recovery_ms), duration(episode.estimated_cost_ms), severity};
        for (int column = 0; column < static_cast<int>(values.size()); ++column) {
            distractionsTable_->setItem(row, column, new QTableWidgetItem(values[column]));
        }
    }
}

void MainWindow::refreshWeekly() {
    const storage::DateRange range{localDayStart(55), localDayStart() + kDayMs};
    const auto trends = activity_.weeklyTrends(range);
    weeklyTable_->setRowCount(static_cast<int>(trends.size()));
    for (int row = 0; row < weeklyTable_->rowCount(); ++row) {
        const auto& trend = trends[static_cast<std::size_t>(row)];
        const auto date = QDateTime::fromMSecsSinceEpoch(trend.period_start_unix_ms).date();
        const std::array<QString, 6> values{
            "Week of " + date.toString("MMM d"), duration(trend.metrics.active_ms),
            duration(trend.metrics.focused_ms), duration(trend.metrics.deep_work_ms),
            duration(trend.metrics.distraction_ms),
            QString::number(trend.metrics.context_switch_count)};
        for (int column = 0; column < static_cast<int>(values.size()); ++column) {
            weeklyTable_->setItem(row, column, new QTableWidgetItem(values[column]));
        }
    }
}

void MainWindow::refreshGoals() {
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
            percent(goal.progress), goal.achieved ? "Achieved" : "In progress"};
        for (int column = 0; column < static_cast<int>(values.size()); ++column) {
            goalsTable_->setItem(row, column, new QTableWidgetItem(values[column]));
        }
    }
}

void MainWindow::refreshExperiments() {
    const auto experiments = activity_.experimentResults();
    experimentsTable_->setRowCount(static_cast<int>(experiments.size()));
    for (int row = 0; row < experimentsTable_->rowCount(); ++row) {
        const auto& result = experiments[static_cast<std::size_t>(row)];
        const std::array<QString, 5> values{
            QString::fromStdString(result.experiment.name),
            QString::fromStdString(result.experiment.hypothesis),
            QString::fromStdString(result.experiment.intervention),
            QString::fromStdString(result.experiment.status),
            QString::fromStdString(result.interpretation)};
        for (int column = 0; column < static_cast<int>(values.size()); ++column) {
            experimentsTable_->setItem(row, column, new QTableWidgetItem(values[column]));
        }
    }
}

void MainWindow::refreshRules() {
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

void MainWindow::refreshApplications() {
    const auto applications = activity_.applications();
    applicationsTable_->setRowCount(static_cast<int>(applications.size()));
    for (int row = 0; row < applicationsTable_->rowCount(); ++row) {
        const auto& app = applications[static_cast<std::size_t>(row)];
        applicationsTable_->setItem(
            row, 0, new QTableWidgetItem(QString::fromStdString(app.name)));
        applicationsTable_->setItem(
            row, 1, new QTableWidgetItem(QString::fromStdString(app.category)));
        applicationsTable_->setItem(row, 2,
                                    new QTableWidgetItem(app.is_distraction ? "Yes" : "No"));
    }
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
