#pragma once

#include "dashboard_page.hpp"

class QTableWidget;

namespace activityos::ui {

class AppsPage final : public DashboardPage {
public:
    explicit AppsPage(ActivityService& activity, QWidget* parent = nullptr);

    void refresh(const DashboardSnapshot& snapshot) override;

private:
    void reload();

    ActivityService& activity_;

    QTableWidget* appsTable_{};
    QTableWidget* rulesTable_{};
};

} // namespace activityos::ui
