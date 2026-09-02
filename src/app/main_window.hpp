#pragma once

#include "activityos/services.hpp"

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

namespace activityos::ui {

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(storage::Database& database, QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

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
    QLabel* activeApplication_{};
    QLabel* focusedValue_{};
    QLabel* deepWorkValue_{};
    QLabel* distractionValue_{};
    QLabel* switchesValue_{};
    QLabel* scoreValue_{};
    QLabel* workdayValue_{};
    QTableWidget* categoryTable_{};
    QListWidget* todayInsights_{};

    QTableWidget* historyTable_{};
    QTableWidget* transitionsTable_{};
    QTableWidget* distractionsTable_{};
    QLabel* profileText_{};
    QTableWidget* weeklyTable_{};
    QTableWidget* goalsTable_{};
    QTableWidget* experimentsTable_{};
    QListWidget* insightsList_{};
    QTableWidget* rulesTable_{};
    QTableWidget* applicationsTable_{};

    QCheckBox* pauseTracking_{};
    QCheckBox* storeTitles_{};
    QSpinBox* idleThreshold_{};

    QWidget* buildTodayPage();
    QWidget* buildHistoryPage();
    QWidget* buildAnalyticsPage();
    QWidget* buildWeeklyPage();
    QWidget* buildGoalsPage();
    QWidget* buildExperimentsPage();
    QWidget* buildInsightsPage();
    QWidget* buildRulesPage();
    QWidget* buildPrivacyPage();
    QWidget* buildSettingsPage();
    void buildShell();
    void buildTray();
    void refresh();
    void refreshTracker();
    void refreshToday(const DashboardSnapshot& snapshot);
    void refreshHistory();
    void refreshAnalytics(const DashboardSnapshot& snapshot);
    void refreshWeekly();
    void refreshGoals();
    void refreshExperiments();
    void refreshRules();
    void refreshApplications();
    void loadDemoData();
    storage::DateRange todayRange() const;
};

} // namespace activityos::ui
