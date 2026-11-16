#include "activityos/services.hpp"

#include <chrono>
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
    std::optional<std::string> title = std::nullopt,
    std::optional<std::string> url = std::nullopt) {
    activityos::ActivitySnapshot value;
    value.application_name = std::move(app);
    value.window_title = std::move(title);
    value.browser_url = std::move(url);
    value.idle_duration = std::chrono::milliseconds(idle_ms);
    value.metadata.status = activityos::ActivitySourceStatus::available;
    value.metadata.capabilities = {true, true, true, true};
    return value;
}

// Milliseconds since the Unix epoch for a UTC date and time.
std::int64_t utcMs(int year, unsigned month, unsigned day, int hours, int minutes = 0) {
    using namespace std::chrono;
    const sys_days date{std::chrono::year{year} / std::chrono::month{month} /
                        std::chrono::day{day}};
    return duration_cast<milliseconds>(date.time_since_epoch()).count() +
           (hours * 60LL + minutes) * 60 * 1000;
}

// US Central time in 2026: UTC-6, and UTC-5 between 8 March 02:00 CST
// (08:00 UTC) and 1 November 02:00 CDT (07:00 UTC).
int usCentral2026OffsetMinutes(activityos::UnixMillis timestamp) {
    const bool daylight =
        timestamp >= utcMs(2026, 3, 8, 8) && timestamp < utcMs(2026, 11, 1, 7);
    return daylight ? -5 * 60 : -6 * 60;
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

    auto codeSource = std::make_unique<FakeActivitySource>(
        std::vector<ActivitySnapshot>{snapshot("Code")});
    TrackerService codeTracker(browserDatabase, std::move(codeSource));
    codeTracker.poll(start - 60 * 1000);
    codeTracker.shutdown(start);
    const auto codeApp = browserDatabase.applicationByName("Code");
    check(codeApp && codeApp->category == "Coding" && !codeApp->is_distraction,
          "VS Code (Code) is classified as coding");

    check(chatgpt && chatgpt->category == "Research" && !chatgpt->is_distraction,
          "AI companion apps are classified as research");

    auto browserSource = std::make_unique<FakeActivitySource>(
        std::vector<ActivitySnapshot>{
            snapshot("Google Chrome", 0, "Linear Algebra Lecture - YouTube"),
            snapshot("Google Chrome", 0, "ActivityOS repository - GitHub"),
            snapshot("Google Chrome", 0, "Physics Sheet - Course Documents"),
            snapshot("Google Chrome", 0, "Untitled document - Google Docs"),
            snapshot("Visual Studio Code")});
    TrackerService browserTracker(browserDatabase, std::move(browserSource));
    browserTracker.poll(start);
    browserTracker.poll(start + 5 * 60 * 1000);
    browserTracker.poll(start + 10 * 60 * 1000);
    browserTracker.poll(start + 15 * 60 * 1000);
    browserTracker.poll(start + 20 * 60 * 1000);
    browserTracker.shutdown(start + 21 * 60 * 1000);
    const auto browserSessions =
        browserDatabase.sessions({start, start + 22 * 60 * 1000});
    check(browserSessions.size() == 4 &&
              browserSessions[0].category == "Entertainment" &&
              browserSessions[1].category == "Research" &&
              browserSessions[2].category == "Work" &&
              browserSessions[3].category == "Coding",
          "schoolwork and browser documents share one Work session");

    auto urlSource = std::make_unique<FakeActivitySource>(
        std::vector<ActivitySnapshot>{
            snapshot("Google Chrome", 0, "Untitled",
                       "https://docs.google.com/document/d/abc/edit"),
            snapshot("Google Chrome", 0, "Slides",
                       "https://courses.grainger.illinois.edu/cs225/fa2026/assets/lectures/"
                       "slides/cs225fa26-05-linked3-slides.pdf"),
            snapshot("Google Chrome", 0, "Watch later",
                       "https://www.youtube.com/watch?v=abc")});
    TrackerService urlTracker(browserDatabase, std::move(urlSource));
    urlTracker.poll(start + 30 * 60 * 1000);
    urlTracker.poll(start + 35 * 60 * 1000);
    urlTracker.poll(start + 40 * 60 * 1000);
    urlTracker.shutdown(start + 41 * 60 * 1000);
    const auto urlSessions =
        browserDatabase.sessions({start + 30 * 60 * 1000, start + 42 * 60 * 1000});
    check(urlSessions.size() == 2 && urlSessions[0].category == "Work" &&
              urlSessions[1].category == "Entertainment",
          "browser URLs classify docs, school sites, and YouTube");

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

    storage::Database lockDatabase = storage::Database::inMemory();
    lockDatabase.migrate();
    auto lockSource = std::make_unique<FakeActivitySource>(
        std::vector<ActivitySnapshot>{snapshot("loginwindow"), snapshot("loginwindow"),
                                      snapshot("Google Chrome")});
    TrackerService lockTracker(lockDatabase, std::move(lockSource));
    lockTracker.poll(start);
    lockTracker.poll(start + 10 * 60 * 1000);
    lockTracker.poll(start + 11 * 60 * 1000);
    lockTracker.shutdown(start + 20 * 60 * 1000);
    const storage::DateRange lockRange{start, start + 30 * 60 * 1000};
    check(lockDatabase.isApplicationExcluded("loginwindow"),
          "loginwindow is auto-excluded");
    bool loginSession = false;
    bool chromeSession = false;
    const auto loginApp = lockDatabase.applicationByName("loginwindow");
    const auto chromeApp = lockDatabase.applicationByName("Google Chrome");
    for (const auto& session : lockDatabase.sessions(lockRange)) {
        if (loginApp && session.application_id == loginApp->id) loginSession = true;
        if (chromeApp && session.application_id == chromeApp->id) chromeSession = true;
    }
    check(!loginSession, "loginwindow does not create active sessions");
    check(chromeSession, "real apps still create sessions after unlock");

    // Default classification matches whole names or whole words, never a
    // substring of a longer word. The first block lists real applications that
    // substring matching used to put in the wrong category.
    struct ClassificationCase {
        const char* application;
        const char* category;
        bool distraction{false};
        std::optional<std::string> title{};
    };
    const std::vector<ClassificationCase> classificationCases{
        {"Archive Utility", "Other"},
        {"The Unarchiver", "Other"},
        {"ArcGIS Pro", "Other"},
        {"Search", "Other"},
        {"Knowledge", "Other"},
        {"Zedge", "Other"},
        {"Poedit", "Other"},
        {"Novabench", "Other"},
        {"Nova Launcher", "Other"},
        {"Gemini 2: The Duplicate Finder", "Other"},
        {"ZoomIt64.exe", "Other"},
        {"Cursor Pro", "Other"},
        // A name that merely contains "edge" is not a browser, so its window
        // title must not be used for classification.
        {"Knowledge Base", "Other", false, "Lecture 4 - YouTube"},

        {"Code", "Coding"},
        {"Code.exe", "Coding"},
        {"Visual Studio Code", "Coding"},
        {"Cursor", "Coding"},
        {"Xcode", "Coding"},
        {"Terminal", "Coding"},
        {"iTerm2", "Coding"},
        {"WindowsTerminal.exe", "Coding"},
        {"gnome-terminal-server", "Coding"},
        {"IntelliJ IDEA", "Coding"},
        {"PyCharm CE", "Coding"},
        {"pycharm64.exe", "Coding"},
        {"CLion", "Coding"},
        {"Android Studio", "Coding"},
        {"Sublime Text", "Coding"},
        {"sublime_text.exe", "Coding"},
        {"Nova", "Coding"},
        {"Zed", "Coding"},

        {"ChatGPT", "Research"},
        {"Claude", "Research"},
        {"Gemini", "Research"},
        {"Perplexity", "Research"},
        {"Poe", "Research"},
        {"GitHub Copilot", "Research"},

        {"Slack", "Communication"},
        {"Discord", "Communication"},
        {"Microsoft Teams", "Communication"},
        {"zoom.us", "Communication"},
        {"Mail", "Communication"},
        {"Airmail", "Communication"},
        {"Messages", "Communication"},

        {"Spotify", "Entertainment", true},
        {"Steam", "Entertainment", true},
        {"steamwebhelper.exe", "Entertainment", true},
        {"Netflix", "Entertainment", true},

        {"Notion", "Planning"},
        {"Obsidian", "Planning"},
        {"Todoist", "Planning"},
        {"Calendar", "Planning"},

        {"Google Chrome", "General"},
        {"chrome.exe", "General"},
        {"Safari", "General"},
        {"Firefox", "General"},
        {"Microsoft Edge", "General"},
        {"msedge.exe", "General"},
        {"Arc", "General"},
        // Real browsers still get title-based classification.
        {"Microsoft Edge Beta", "Entertainment", true, "Lecture 4 - YouTube"},
    };
    storage::Database classificationDatabase = storage::Database::inMemory();
    classificationDatabase.migrate();
    std::vector<ActivitySnapshot> classificationSnapshots;
    for (const auto& item : classificationCases) {
        classificationSnapshots.push_back(snapshot(item.application, 0, item.title));
    }
    TrackerService classificationTracker(
        classificationDatabase,
        std::make_unique<FakeActivitySource>(std::move(classificationSnapshots)));
    for (std::size_t i = 0; i < classificationCases.size(); ++i) {
        classificationTracker.poll(start + static_cast<std::int64_t>(i) * 5 * 1000);
    }
    for (const auto& item : classificationCases) {
        const auto stored = classificationDatabase.applicationByName(item.application);
        const std::string message = std::string("default classification of \"") +
                                    item.application + "\" is " + item.category +
                                    (stored ? ", got " + stored->category : ", got nothing");
        check(stored && stored->category == item.category &&
                  stored->is_distraction == item.distraction,
              message.c_str());
    }

    // Daily history steps by local calendar day, so sessions next to midnight
    // stay in the right day when a daylight-saving change makes a day 23 or 25
    // hours long.
    storage::Database dstDatabase = storage::Database::inMemory();
    dstDatabase.migrate();
    storage::Application editor;
    editor.name = "Xcode";
    editor.category = "Coding";
    editor.created_at = 1;
    const auto editorId = dstDatabase.upsertApplication(editor);
    const auto addCodingSession = [&](std::int64_t sessionStart, std::int64_t durationMs) {
        dstDatabase.addSession({0, editorId, "Coding", sessionStart,
                                sessionStart + durationMs, durationMs, 0, 0, false, false});
    };
    constexpr std::int64_t kMinuteMs = 60 * 1000;
    // 00:10 CDT on 9 March, the day after the 23-hour day.
    addCodingSession(utcMs(2026, 3, 9, 5, 10), 20 * kMinuteMs);
    // 23:30 CST on 1 November, the last half hour of the 25-hour day.
    addCodingSession(utcMs(2026, 11, 2, 5, 30), 15 * kMinuteMs);

    AnalyticsConfig centralConfig;
    centralConfig.utc_offset_minutes_at = usCentral2026OffsetMinutes;
    ActivityService centralActivity(dstDatabase, centralConfig);

    const auto springDays =
        centralActivity.dailyHistory({utcMs(2026, 3, 7, 6), utcMs(2026, 3, 10, 5)});
    check(springDays.size() == 3 &&
              springDays[0].day_start_unix_ms == utcMs(2026, 3, 7, 6) &&
              springDays[1].day_start_unix_ms == utcMs(2026, 3, 8, 6) &&
              springDays[2].day_start_unix_ms == utcMs(2026, 3, 9, 5),
          "daily history starts each day at local midnight across spring forward");
    check(springDays.size() == 3 && springDays[1].active_ms == 0 &&
              springDays[2].active_ms == 20 * kMinuteMs,
          "a session just after midnight on 9 March is not counted on 8 March");

    const auto fallDays =
        centralActivity.dailyHistory({utcMs(2026, 10, 31, 5), utcMs(2026, 11, 3, 6)});
    check(fallDays.size() == 3 &&
              fallDays[0].day_start_unix_ms == utcMs(2026, 10, 31, 5) &&
              fallDays[1].day_start_unix_ms == utcMs(2026, 11, 1, 5) &&
              fallDays[2].day_start_unix_ms == utcMs(2026, 11, 2, 6),
          "daily history starts each day at local midnight across fall back");
    check(fallDays.size() == 3 && fallDays[1].active_ms == 15 * kMinuteMs &&
              fallDays[2].active_ms == 0,
          "a session late on 1 November is not counted on 2 November");

    ActivityService fixedOffsetActivity(dstDatabase);
    check(fixedOffsetActivity.dailyHistory({0, 3 * 24 * 60 * kMinuteMs}).size() == 3,
          "daily history keeps 24-hour days with a fixed offset");

    if (failures == 0) {
        std::cout << "All service tests passed.\n";
        return 0;
    }
    std::cerr << failures << " service test(s) failed.\n";
    return 1;
}
