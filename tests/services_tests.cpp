#include "activityos/services.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

activityos::ActivitySnapshot snapshot(
    std::string app,
    std::int64_t idle_ms = 0,
    std::optional<std::string> title = std::nullopt) {
    activityos::ActivitySnapshot value;
    value.application_name = std::move(app);
    value.window_title = std::move(title);
    value.idle_duration = std::chrono::milliseconds(idle_ms);
    value.metadata.status = activityos::ActivitySourceStatus::available;
    value.metadata.capabilities = {true, true, true, true};
    return value;
}

} // namespace

int main() {
    using namespace activityos;
    storage::Database database = storage::Database::inMemory();
    database.migrate();

    storage::ClassificationRule rule;
    rule.application_pattern = "Code";
    rule.category = "Coding";
    rule.priority = 100;
    rule.created_at = rule.updated_at = 1;
    database.saveClassificationRule(rule);

    constexpr std::int64_t start = 1'700'000'000'000;
    auto source = std::make_unique<FakeActivitySource>(
        std::vector<ActivitySnapshot>{snapshot("Visual Studio Code"),
                                      snapshot("Visual Studio Code"),
                                      snapshot("Safari"),
                                      snapshot("Safari", 6 * 60 * 1000),
                                      snapshot("Visual Studio Code")});
    TrackerService tracker(database, std::move(source));
    tracker.poll(start);
    tracker.poll(start + 25 * 60 * 1000);
    tracker.poll(start + 26 * 60 * 1000);
    tracker.poll(start + 32 * 60 * 1000);
    tracker.poll(start + 33 * 60 * 1000);
    tracker.shutdown(start + 55 * 60 * 1000);

    const storage::DateRange range{start, start + 60 * 60 * 1000};
    check(database.activityEvents(range).size() >= 4,
          "tracker records transitions, idle boundaries, and heartbeats");
    check(database.sessions(range).size() >= 2, "tracker persists completed sessions");
    check(database.contextSwitches(range).size() == 1, "tracker detects app switch");
    check(database.idlePeriods(range).size() == 1, "tracker records idle period");

    ActivityService activity(database);
    const auto dashboard = activity.dashboard(range, start);
    check(dashboard.metrics.active_ms > 0, "dashboard aggregates active time");
    check(dashboard.metrics.focused_ms > 0, "dashboard detects focus session");
    check(!activity.applications().empty(), "application inventory is available");

    database.excludeApplication("Visual Studio Code");
    check(database.isApplicationExcluded("Visual Studio Code"),
          "privacy exclusion is persisted");

    storage::Database browserDatabase = storage::Database::inMemory();
    browserDatabase.migrate();
    auto assistantSource = std::make_unique<FakeActivitySource>(
        std::vector<ActivitySnapshot>{
            snapshot("Cursor"),
            snapshot("ChatGPT")});
    TrackerService assistantTracker(browserDatabase, std::move(assistantSource));
    assistantTracker.poll(start - 3 * 60 * 1000);
    assistantTracker.poll(start - 2 * 60 * 1000);
    assistantTracker.shutdown(start - 60 * 1000);
    const auto cursor = browserDatabase.applicationByName("Cursor");
    const auto chatgpt = browserDatabase.applicationByName("ChatGPT");
    check(cursor && cursor->category == "Coding" && !cursor->is_distraction,
          "Cursor is classified as coding");
    check(chatgpt && chatgpt->category == "Research" && !chatgpt->is_distraction,
          "AI companion apps are classified as research");

    auto browserSource = std::make_unique<FakeActivitySource>(
        std::vector<ActivitySnapshot>{
            snapshot("Google Chrome", 0, "Linear Algebra Lecture - YouTube"),
            snapshot("Google Chrome", 0, "ActivityOS repository - GitHub"),
            snapshot("Visual Studio Code")});
    TrackerService browserTracker(browserDatabase, std::move(browserSource));
    browserTracker.poll(start);
    browserTracker.poll(start + 5 * 60 * 1000);
    browserTracker.poll(start + 10 * 60 * 1000);
    browserTracker.shutdown(start + 11 * 60 * 1000);
    const auto browserSessions =
        browserDatabase.sessions({start, start + 12 * 60 * 1000});
    check(browserSessions.size() >= 2 &&
              browserSessions[0].category == "Entertainment" &&
              browserSessions[1].category == "Research",
          "browser title changes split entertainment and productive sessions");

    storage::ClassificationRule studyOverride;
    studyOverride.application_pattern = "Chrome";
    studyOverride.title_pattern = "YouTube";
    studyOverride.category = "Research";
    studyOverride.priority = 100;
    studyOverride.created_at = studyOverride.updated_at = start;
    browserDatabase.saveClassificationRule(studyOverride);
    auto studySource = std::make_unique<FakeActivitySource>(
        std::vector<ActivitySnapshot>{
            snapshot("Google Chrome", 0, "CS50 Lecture - YouTube"),
            snapshot("Visual Studio Code")});
    TrackerService studyTracker(browserDatabase, std::move(studySource));
    studyTracker.poll(start + 10 * 60 * 1000);
    studyTracker.poll(start + 11 * 60 * 1000);
    studyTracker.shutdown(start + 12 * 60 * 1000);
    const auto overriddenChrome = browserDatabase.applicationByName("Google Chrome");
    check(overriddenChrome && overriddenChrome->category == "Research" &&
              !overriddenChrome->is_distraction,
          "user browser rules override built-in title classification");

    if (failures == 0) {
        std::cout << "All service tests passed.\n";
        return 0;
    }
    std::cerr << failures << " service test(s) failed.\n";
    return 1;
}
