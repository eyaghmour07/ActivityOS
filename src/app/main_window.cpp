#include "main_window.hpp"

#include "apps_page.hpp"
#include "focus_page.hpp"
#include "goals_page.hpp"
#include "settings_page.hpp"
#include "style.hpp"
#include "today_page.hpp"
#include "trends_page.hpp"
#include "ui_helpers.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

#include <exception>

namespace activityos::ui {

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
    todayPage_ = new TodayPage(activity_);
    dashboardPages_ = {todayPage_, new FocusPage(activity_), new AppsPage(activity_),
                       new TrendsPage(activity_), new GoalsPage(activity_, database_)};
    // Built last: the settings page applies the saved settings as it is
    // constructed, which can pause the tracker and redraw the tracking status.
    settingsPage_ = new SettingsPage(activity_, database_, *tracker_,
                                     {[this] { refresh(); }, [this] { refreshTracker(); }});
    dashboardPages_.push_back(settingsPage_);
    for (auto* dashboardPage : dashboardPages_) pages_->addWidget(dashboardPage);
    connect(navigation_, &QListWidget::currentRowChanged, pages_,
            &QStackedWidget::setCurrentIndex);

    shell->addWidget(sidebar);
    shell->addWidget(pages_, 1);
    setCentralWidget(central);
    qApp->setStyleSheet(friendlyStyleSheet(qApp->palette()));
    statusBar()->showMessage("Local-only · everything stays on this device");
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
        settingsPage_->setTrackingPaused(value);
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

void MainWindow::refreshTracker() {
    const auto status = tracker_->poll(unixMillisecondsNow());
    if (status.paused) {
        todayPage_->setTrackingState("PAUSED", false);
        sidebarTrayStatus_->setText("Paused");
    } else if (status.idle) {
        todayPage_->setTrackingState("IDLE", false);
        sidebarTrayStatus_->setText("Idle");
    } else if (status.source_status == ActivitySourceStatus::unsupported) {
        todayPage_->setTrackingState("UNAVAILABLE", false);
        sidebarTrayStatus_->setText("Unavailable");
    } else {
        todayPage_->setTrackingState("RECORDING", true);
        sidebarTrayStatus_->setText("Active");
    }

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
        for (auto* dashboardPage : dashboardPages_) dashboardPage->refresh(snapshot);
        statusBar()->showMessage("Updated " + QTime::currentTime().toString("h:mm:ss AP"));
    } catch (const std::exception& error) {
        statusBar()->showMessage("Update failed: " + QString::fromUtf8(error.what()));
    }
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    for (auto* dashboardPage : dashboardPages_) dashboardPage->redrawCharts();
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
