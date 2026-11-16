#pragma once

#include "dashboard_page.hpp"

#include <functional>

class QCheckBox;
class QSpinBox;

namespace activityos::ui {

class SettingsPage final : public DashboardPage {
public:
    struct Callbacks {
        std::function<void()> data_changed;     // activity was deleted or demo data loaded
        std::function<void()> tracking_changed; // tracking was paused or resumed
    };

    SettingsPage(ActivityService& activity, storage::Database& database,
                 TrackerService& tracker, Callbacks callbacks, QWidget* parent = nullptr);

    void refresh(const DashboardSnapshot& snapshot) override;
    // Mirrors the tray's pause action into the "Pause tracking" checkbox.
    void setTrackingPaused(bool paused);

private:
    ActivityService& activity_;
    storage::Database& database_;
    TrackerService& tracker_;
    Callbacks callbacks_;

    QCheckBox* pauseTracking_{};
    QCheckBox* storeTitles_{};
    QSpinBox* idleThreshold_{};
    QCheckBox* launchAtLogin_{};
};

} // namespace activityos::ui
