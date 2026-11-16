#pragma once

#include "dashboard_page.hpp"

class QLabel;

namespace activityos::ui {

class AppBreakdownWidget;
class DayTimelineWidget;

class TodayPage final : public DashboardPage {
public:
    explicit TodayPage(ActivityService& activity, QWidget* parent = nullptr);

    void refresh(const DashboardSnapshot& snapshot) override;
    void redrawCharts() override;
    // Updates the recording indicator in the page header.
    void setTrackingState(const QString& text, bool recording);

private:
    ActivityService& activity_;

    QLabel* trackingState_{};
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
};

} // namespace activityos::ui
