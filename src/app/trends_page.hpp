#pragma once

#include "dashboard_page.hpp"

class QLabel;
class QTableWidget;

namespace activityos::ui {

class WeeklyChartWidget;

class TrendsPage final : public DashboardPage {
public:
    explicit TrendsPage(ActivityService& activity, QWidget* parent = nullptr);

    void refresh(const DashboardSnapshot& snapshot) override;
    void redrawCharts() override;

private:
    ActivityService& activity_;

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
};

} // namespace activityos::ui
