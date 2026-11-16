#include "activityos/storage.hpp"

// Keep assert() active in Release builds; these suites rely on it.
#undef NDEBUG
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace activityos::storage;

int main() {
    Database db = Database::inMemory();
    assert(db.isOpen());
    assert(db.appliedMigrations().empty());
    db.migrate();

    // The schema comes only from resources/migrations/*.sql, applied in order.
    const auto built_in = builtInMigrations();
    assert(!built_in.empty());
    assert(built_in.front().version == 1);
    assert(built_in.front().name == "001_initial");
    std::vector<int> built_in_versions;
    for (const auto& migration : built_in) built_in_versions.push_back(migration.version);
    assert(db.appliedMigrations() == built_in_versions);
    db.migrate();
    assert(db.appliedMigrations() == built_in_versions);

    db.setSetting("idle_threshold_seconds", "300");
    assert(db.setting("idle_threshold_seconds") == "300");
    db.removeSetting("idle_threshold_seconds");
    assert(!db.setting("idle_threshold_seconds"));

    Application vscode;
    vscode.name = "Visual Studio Code";
    vscode.category = "Coding";
    vscode.created_at = 100;
    const auto vscode_id = db.upsertApplication(vscode);

    Application browser;
    browser.name = "Browser";
    browser.category = "Research";
    browser.created_at = 101;
    const auto browser_id = db.upsertApplication(browser);
    assert(db.applications().size() == 2);
    assert(db.applicationByName("visual studio code")->id == vscode_id);

    vscode.id = vscode_id;
    vscode.category = "Development";
    assert(db.upsertApplication(vscode) == vscode_id);
    assert(db.application(vscode_id)->category == "Development");

    ClassificationRule rule;
    rule.application_pattern = "Browser";
    rule.title_pattern = "*YouTube*";
    rule.category = "Entertainment";
    rule.is_distraction = true;
    rule.priority = 50;
    const auto rule_id = db.saveClassificationRule(rule);
    assert(db.classificationRules().front().id == rule_id);
    rule.id = rule_id;
    rule.priority = 100;
    db.saveClassificationRule(rule);
    assert(db.classificationRules().front().priority == 100);

    db.excludeApplication("Private Browser");
    db.excludeCategory("Personal");
    assert(db.isApplicationExcluded("private browser"));
    assert(db.isCategoryExcluded("personal"));
    db.includeApplication("Private Browser");
    db.includeCategory("Personal");
    assert(!db.isApplicationExcluded("Private Browser"));
    assert(!db.isCategoryExcluded("Personal"));

    db.addActivityEvent({0, vscode_id, 1'000, "ACTIVE", "main.cpp"});
    db.addActivityEvent({0, browser_id, 2'000, "SWITCH", "SQLite documentation"});
    db.addActivityEvent({0, std::nullopt, 9'000, "RESUME", ""});
    db.addIdlePeriod({0, 3'000, 4'000, 1'000});
    db.addSession({0, vscode_id, "Coding", 1'000, 3'000, 2'000, 1, 1, true, false});
    db.addContextSwitch({0, vscode_id, browser_id, 2'000});

    const DateRange first_hour{0, 5'000};
    assert(db.activityEvents(first_hour).size() == 2);
    assert(db.idlePeriods(first_hour).size() == 1);
    assert(db.sessions(first_hour).front().is_focus);
    assert(db.contextSwitches(first_hour).size() == 1);

    const std::string csv = db.exportCsv(first_hour);
    assert(csv.find("timestamp,application,category,event_type,window_title") == 0);
    assert(csv.find("Visual Studio Code") != std::string::npos);
    const std::string json = db.exportJson(first_hour);
    assert(json.find("\"activity_events\"") != std::string::npos);
    assert(json.find("\"main.cpp\"") != std::string::npos);

    Goal goal;
    goal.name = "Three focused hours";
    goal.metric = "focus_ms";
    goal.target_value = 10'800'000;
    goal.created_at = 500;
    const auto goal_id = db.saveGoal(goal);
    assert(db.goals(true).front().id == goal_id);

    Experiment experiment;
    experiment.name = "Quiet mornings";
    experiment.hypothesis = "Fewer notifications improve focus";
    experiment.intervention = "Mute notifications";
    experiment.start_time = 1'000;
    experiment.status = "running";
    experiment.baseline_value = 120.0;
    experiment.created_at = 500;
    const auto experiment_id = db.saveExperiment(experiment);
    assert(db.experiments().front().id == experiment_id);

    Insight insight;
    insight.kind = "fragmentation";
    insight.title = "More switches";
    insight.message = "Switching exceeded your baseline.";
    insight.score = 0.8;
    insight.generated_at = 2'500;
    const auto insight_id = db.saveInsight(insight);
    assert(db.insights(first_hour).front().id == insight_id);
    db.dismissInsight(insight_id);
    assert(db.insights(first_hour).front().dismissed);

    {
        Transaction transaction(db);
        db.addActivityEvent({0, vscode_id, 4'500, "ACTIVE", "rolled back"});
        transaction.rollback();
    }
    assert(db.activityEvents(first_hour).size() == 2);

    db.deleteRange({1'500, 3'500});
    assert(db.activityEvents(first_hour).size() == 1);
    assert(db.sessions(first_hour).empty());
    assert(db.idlePeriods(first_hour).empty());
    assert(db.contextSwitches(first_hour).empty());

    assert(db.cleanupRetention(8'000) == 1);
    assert(db.activityEvents({0, 10'000}).size() == 1);
    db.deleteAllActivity();
    assert(db.activityEvents({0, 10'000}).empty());

    bool invalid_range_rejected = false;
    try {
        (void)db.activityEvents({2, 1});
    } catch (const std::invalid_argument&) {
        invalid_range_rejected = true;
    }
    assert(invalid_range_rejected);

    db.removeClassificationRule(rule_id);
    db.removeGoal(goal_id);
    db.removeExperiment(experiment_id);
    db.removeInsight(insight_id);
    assert(db.classificationRules().empty());
    assert(db.goals().empty());
    assert(db.experiments().empty());
    assert(db.insights(first_hour).empty());

    // A later migration applies on top of an existing v0.1 database, which is
    // 001_initial plus the user's data, and leaves that data in place.
    Database legacy = Database::inMemory();
    legacy.migrate(built_in.first(1));
    assert(legacy.appliedMigrations() == std::vector<int>{1});
    Application editor;
    editor.name = "Xcode";
    editor.category = "Coding";
    editor.created_at = 100;
    const auto editor_id = legacy.upsertApplication(editor);
    legacy.addSession({0, editor_id, "Coding", 1'000, 3'000, 2'000, 0, 0, false, false});

    const Migration add_notes{
        2, "002_session_notes",
        "ALTER TABLE sessions ADD COLUMN notes TEXT NOT NULL DEFAULT '';"
        "INSERT INTO settings(key,value,updated_at) VALUES('migration_probe','2',0);"};
    // Passed out of order on purpose; migrations run by version.
    const std::vector<Migration> with_notes{add_notes, built_in.front()};
    legacy.migrate(with_notes);
    assert((legacy.appliedMigrations() == std::vector<int>{1, 2}));
    assert(legacy.setting("migration_probe") == "2");
    assert(legacy.sessions({0, 5'000}).size() == 1);
    assert(legacy.applicationByName("Xcode")->id == editor_id);
    // Applying again must skip 002: adding the column twice would fail.
    legacy.migrate(with_notes);
    assert((legacy.appliedMigrations() == std::vector<int>{1, 2}));

    // A fresh database gets the same result from the full list.
    Database fresh = Database::inMemory();
    fresh.migrate(with_notes);
    assert((fresh.appliedMigrations() == std::vector<int>{1, 2}));

    // A failing migration is rolled back completely and is not recorded.
    const Migration broken{
        3, "003_broken",
        "INSERT INTO settings(key,value,updated_at) VALUES('half_applied','1',0);"
        "INSERT INTO missing_table(value) VALUES(1);"};
    const std::vector<Migration> with_broken{built_in.front(), add_notes, broken};
    bool broken_migration_reported = false;
    try {
        legacy.migrate(with_broken);
    } catch (const std::runtime_error& error) {
        broken_migration_reported =
            std::string(error.what()).find("003_broken") != std::string::npos;
    }
    assert(broken_migration_reported);
    assert((legacy.appliedMigrations() == std::vector<int>{1, 2}));
    assert(!legacy.setting("half_applied"));

    bool duplicate_version_rejected = false;
    try {
        const std::vector<Migration> duplicated{built_in.front(), add_notes, add_notes};
        legacy.migrate(duplicated);
    } catch (const std::invalid_argument&) {
        duplicate_version_rejected = true;
    }
    assert(duplicate_version_rejected);

    std::cout << "ActivityOS storage tests passed\n";
    return 0;
}
