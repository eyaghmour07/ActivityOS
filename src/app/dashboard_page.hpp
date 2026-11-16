#pragma once

#include "activityos/services.hpp"

#include <QScrollArea>

class QVBoxLayout;

namespace activityos::ui {

// One page of the dashboard: a scrolling column of content that redraws itself
// from a snapshot. MainWindow owns the pages and refreshes them together.
class DashboardPage : public QScrollArea {
public:
    virtual void refresh(const DashboardSnapshot& snapshot) = 0;
    // Called when the window is resized, for pages with custom-painted charts.
    virtual void redrawCharts() {}

protected:
    explicit DashboardPage(QWidget* parent = nullptr);
    [[nodiscard]] QVBoxLayout* contentLayout() const noexcept { return content_layout_; }

private:
    QVBoxLayout* content_layout_{};
};

} // namespace activityos::ui
