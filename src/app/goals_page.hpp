#pragma once

#include "dashboard_page.hpp"

class QFrame;
class QLabel;
class QTableWidget;
class QVBoxLayout;

namespace activityos::ui {

class GoalsPage final : public DashboardPage {
public:
    GoalsPage(ActivityService& activity, storage::Database& database,
              QWidget* parent = nullptr);

    void refresh(const DashboardSnapshot& snapshot) override;

private:
    void reloadGoals();
    void reloadExperiment();

    ActivityService& activity_;
    storage::Database& database_;

    QVBoxLayout* goalsCardsLayout_{};
    QFrame* experimentCard_{};
    QLabel* experimentTitle_{};
    QLabel* experimentBody_{};
    QLabel* experimentBefore_{};
    QLabel* experimentAfter_{};
    QLabel* experimentDelta_{};
    QTableWidget* goalsTable_{};
    QTableWidget* experimentsTable_{};
};

} // namespace activityos::ui
