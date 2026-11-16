#include "demo_data.hpp"

#include "activityos/services.hpp"
#include "ui_helpers.hpp"

#include <QMessageBox>

#include <array>
#include <optional>

namespace activityos::ui {

void loadDemoData(storage::Database& database, QWidget* parent) {
    if (database.setting("demo_data_version") == "1") {
        QMessageBox::information(parent, "Demo Data", "Demo data is already loaded.");
        return;
    }
    if (QMessageBox::question(
            parent, "Load Demo Data",
            "Add two weeks of clearly labeled synthetic activity to preview every dashboard?") !=
        QMessageBox::Yes) {
        return;
    }

    struct DemoApp {
        const char* name;
        const char* category;
        bool distraction;
        std::int64_t id{};
    };
    std::array<DemoApp, 5> apps{{
        {"Demo IDE", "Coding", false},
        {"Demo Browser Docs", "Research", false},
        {"Demo Team Chat", "Communication", false},
        {"Demo Video", "Entertainment", true},
        {"Demo Planner", "Planning", false},
    }};
    const auto now = unixMillisecondsNow();
    for (auto& app : apps) {
        storage::Application value;
        value.name = app.name;
        value.category = app.category;
        value.is_distraction = app.distraction;
        value.created_at = now;
        app.id = database.upsertApplication(value);
    }

    storage::Transaction transaction(database);
    for (int daysAgo = 13; daysAgo >= 0; --daysAgo) {
        const auto day = localDayStart(daysAgo);
        const auto variation = static_cast<std::int64_t>((13 - daysAgo) % 4) * 8 * kMinuteMs;
        struct Block {
            int app;
            std::int64_t start;
            std::int64_t length;
        };
        const std::array<Block, 7> blocks{{
            {4, 8 * kHourMs + 45 * kMinuteMs, 20 * kMinuteMs},
            {0, 9 * kHourMs + 10 * kMinuteMs, 92 * kMinuteMs + variation},
            {1, 11 * kHourMs, 38 * kMinuteMs},
            {2, 11 * kHourMs + 42 * kMinuteMs, 12 * kMinuteMs},
            {0, 13 * kHourMs, 70 * kMinuteMs + variation / 2},
            {3, 14 * kHourMs + 18 * kMinuteMs, (5 + daysAgo % 3) * kMinuteMs},
            {0, 14 * kHourMs + 30 * kMinuteMs, 48 * kMinuteMs},
        }};
        std::optional<std::int64_t> previous;
        for (const auto& block : blocks) {
            storage::Session session;
            session.application_id = apps[block.app].id;
            session.category = apps[block.app].category;
            session.start_time = day + block.start;
            session.end_time = session.start_time + block.length;
            session.duration_ms = block.length;
            database.addSession(session);
            database.addActivityEvent(
                {0, apps[block.app].id, session.start_time, "DEMO", ""});
            if (previous) {
                database.addContextSwitch(
                    {0, previous, apps[block.app].id, session.start_time});
            }
            previous = apps[block.app].id;
        }
        database.addIdlePeriod(
            {0, day + 12 * kHourMs, day + 13 * kHourMs, kHourMs});
    }

    storage::Goal goal;
    goal.name = "15 focused hours this week";
    goal.metric = "focus_time";
    goal.target_value = 15 * kHourMs;
    goal.period = "weekly";
    goal.created_at = now;
    database.saveGoal(goal);

    storage::Experiment experiment;
    experiment.name = "Quiet mornings";
    experiment.hypothesis = "Muting communication before 11 AM may increase focused work.";
    experiment.intervention = "Mute demo team chat from 9–11 AM for seven days.";
    experiment.start_time = now - 7 * kDayMs;
    experiment.end_time = now;
    experiment.status = "completed";
    experiment.baseline_value = 134.0;
    experiment.result_value = 171.0;
    experiment.notes = "Synthetic demonstration only";
    experiment.created_at = now - 14 * kDayMs;
    database.saveExperiment(experiment);
    database.setSetting("demo_data_version", "1");
    transaction.commit();
    QMessageBox::information(parent, "Demo Data Loaded",
                             "Two weeks of synthetic activity are ready to explore.");
}

} // namespace activityos::ui
