#pragma once

#include "dashboard_page.hpp"

class QLabel;
class QTableWidget;

namespace activityos::ui {

class FocusPage final : public DashboardPage {
public:
    explicit FocusPage(ActivityService& activity, QWidget* parent = nullptr);

    void refresh(const DashboardSnapshot& snapshot) override;

private:
    ActivityService& activity_;

    QLabel* focusTotalValue_{};
    QLabel* focusTotalSub_{};
    QLabel* focusLongestValue_{};
    QLabel* focusLongestSub_{};
    QLabel* focusAvgScoreValue_{};
    QLabel* focusAvgScoreSub_{};
    QLabel* focusFlowValue_{};
    QLabel* focusFlowSub_{};
    QTableWidget* focusSessionsTable_{};
};

} // namespace activityos::ui
