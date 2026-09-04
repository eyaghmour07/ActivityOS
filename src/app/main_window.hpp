#pragma once

#include "activityos/services.hpp"
#include "ui_widgets.hpp"

#include <QMainWindow>

#include <memory>

class QCheckBox;
class QLabel;
class QListWidget;
class QProgressBar;
class QSpinBox;
class QStackedWidget;
class QSystemTrayIcon;
class QTableWidget;
class QTimer;
class QVBoxLayout;

namespace activityos::ui {

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(storage::Database& database, QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    storage::Database& database_;
    ActivityService activity_;
    std::unique_ptr<TrackerService> tracker_;
    QTimer* trackerTimer_{};
    QTimer* refreshTimer_{};
    QSystemTrayIcon* tray_{};
    QListWidget* navigation_{};
    QStackedWidget* pages_{};

    QLabel* trackingState_{};
    QLabel* sidebarTrayStatus_{};
    QLabel* sidebarTrayDetail_{};

    QLabel* activeValue_{};
    QLabel* activeSub_{};
    QLabel* deepWorkValue_{};
    QLabel* deepWorkSub_{};
    QLabel* distractionValue_{};
    QLabel* distractionSub_{};
    QLabel* scoreValue_{};
    QLabel* scoreSub_{};
    QLabel* switchesValue_{};
    QLabel* switchesSub_{};
    QLabel* avgSessionValue_{};
    QLabel* avgSessionSub_{};
    QLabel* recoveryValue_{};
    QLabel* recoverySub_{};
    DayTimelineWidget* dayTimeline_{};
    AppBreakdownWidget* appBreakdown_{};
    QLabel* timelineCaption_{};

    QLabel* focusTotalValue_{};
    QLabel* focusTotalSub_{};
    QLabel* focusLongestValue_{};
    QLabel* focusLongestSub_{};
    QLabel* focusAvgScoreValue_{};
    QLabel* focusAvgScoreSub_{};
    QLabel* focusFlowValue_{};
    QLabel* focusFlowSub_{};
    QTableWidget* focusSessionsTable_{};

    QTableWidget* appsTable_{};
    QTableWidget* rulesTable_{};

    QLabel* trendsAvgFocusValue_{};
    QLabel* trendsAvgFocusSub_{};
    QLabel* trendsBestDayValue_{};
    QLabel* trendsBestDaySub_{};
    QLabel* trendsScoreValue_{};
    QLabel* trendsScoreSub_{};
    QLabel* trendsStreakValue_{};
    QLabel* trendsStreakSub_{};
    WeeklyChartWidget* weeklyChart_{};
    QLabel* workstylePeak_{};
    QLabel* workstyleType_{};
    QLabel* workstyleDayLength_{};
    QLabel* workstyleDeepRatio_{};
    QTableWidget* historyTable_{};

    QVBoxLayout* goalsCardsLayout_{};
    QFrame* experimentCard_{};
    QLabel* experimentTitle_{};
    QLabel* experimentBody_{};
    QLabel* experimentBefore_{};
    QLabel* experimentAfter_{};
    QLabel* experimentDelta_{};
    QTableWidget* goalsTable_{};
    QTableWidget* experimentsTable_{};

    QCheckBox* pauseTracking_{};
    QCheckBox* storeTitles_{};
    QSpinBox* idleThreshold_{};
    QCheckBox* launchAtLogin_{};

    QWidget* buildTodayPage();
    QWidget* buildFocusPage();
    QWidget* buildAppsPage();
    QWidget* buildTrendsPage();
    QWidget* buildGoalsPage();
    QWidget* buildSettingsPage();
    void buildShell();
    void buildTray();
    void refresh();
    void refreshTracker();
    void refreshToday(const DashboardSnapshot& snapshot);
    void refreshFocus(const DashboardSnapshot& snapshot);
    void refreshApps();
    void refreshTrends(const DashboardSnapshot& snapshot);
    void refreshGoals();
    void refreshExperimentCard();
    void loadDemoData();
    storage::DateRange todayRange() const;
};

} // namespace activityos::ui
