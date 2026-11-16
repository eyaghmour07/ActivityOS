#pragma once

#include "activityos/services.hpp"

#include <QMainWindow>

#include <memory>
#include <vector>

class QLabel;
class QListWidget;
class QStackedWidget;
class QSystemTrayIcon;
class QTimer;

namespace activityos::ui {

class DashboardPage;
class SettingsPage;
class TodayPage;

// The application shell: sidebar, tray icon, tracker polling and the timer
// that refreshes every dashboard page from one snapshot.
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

    QLabel* sidebarTrayStatus_{};
    QLabel* sidebarTrayDetail_{};

    TodayPage* todayPage_{};
    SettingsPage* settingsPage_{};
    std::vector<DashboardPage*> dashboardPages_;

    void buildShell();
    void buildTray();
    void refresh();
    void refreshTracker();
};

} // namespace activityos::ui
