#include "activityos/analytics.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

namespace {

using activityos::AnalyticsConfig;
using activityos::AnalyticsEngine;
using activityos::ContextSwitch;
using activityos::DailyMetrics;
using activityos::DistractionEpisode;
using activityos::IdlePeriod;
using activityos::Session;

constexpr std::int64_t minute = 60 * 1000;
constexpr std::int64_t hour = 60 * minute;
constexpr std::int64_t day = 24 * hour;

bool near(double left, double right, double tolerance = 0.001) {
    return std::abs(left - right) <= tolerance;
}

Session makeSession(
    const char* id,
    const char* app,
    const char* category,
    std::int64_t start,
    std::int64_t duration,
    bool productive,
    bool distraction = false) {
    return {
        id,
        app,
        category,
        start,
        start + duration,
        duration,
        productive,
        distraction,
    };
}

void testEmptyAndInvalidInput() {
    AnalyticsEngine engine;
    assert(engine.timeDistribution({}).empty());
    assert(engine.meanSessionMs({}) == 0.0);
    assert(engine.medianSessionMs({}) == 0.0);
    assert((engine.minMaxSessionMs({}) ==
            std::make_pair<std::int64_t, std::int64_t>(0, 0)));
    assert(engine.peakProductiveHalfHour({}) == -1);

    Session invalid = makeSession("bad", "IDE", "Coding", 100, 10, true);
    invalid.end_unix_ms = invalid.start_unix_ms;
    const auto metrics = engine.dailyMetrics(0, {invalid});
    assert(metrics.active_ms == 0);
    assert(metrics.session_count == 0);
    assert(metrics.consistency == 1.0);
}

void testDistributionAndOverlapHandling() {
    AnalyticsEngine engine;
    std::vector<Session> sessions{
        makeSession("a", "IDE", "Coding", 0, 60 * minute, true),
        makeSession("b", "Browser", "Research", 30 * minute, 60 * minute, true),
        makeSession("c", "Video", "Entertainment", 90 * minute, 10 * minute, false, true),
    };
    const auto distribution = engine.timeDistribution(sessions);
    assert(distribution.size() == 3);
    assert(distribution[0].category == "Coding");
    assert(distribution[0].active_ms == 60 * minute);
    assert(distribution[1].category == "Research");
    assert(distribution[1].active_ms == 30 * minute);
    assert(distribution[2].active_ms == 10 * minute);
    assert(near(distribution[0].percentage, 60.0));

    const auto metrics = engine.dailyMetrics(0, sessions);
    assert(metrics.active_ms == 100 * minute);
    assert(metrics.productive_ms == 90 * minute);
    assert(metrics.distraction_ms == 10 * minute);
}

void testProductiveSwitchPreservesFocus() {
    AnalyticsConfig config;
    config.focus_threshold_ms = 20 * minute;
    config.max_focus_switches = 1;
    AnalyticsEngine engine(config);
    std::vector<Session> sessions{
        makeSession("code", "Code", "Coding", 0, 15 * minute, true),
        makeSession("docs", "Chrome", "Work", 15 * minute, 15 * minute, true),
        makeSession("code2", "Code", "Coding", 30 * minute, 15 * minute, true),
    };
    std::vector<ContextSwitch> switches{
        {"Code", "Chrome", 15 * minute},
        {"Chrome", "Code", 30 * minute},
    };
    const auto focus = engine.focusSessions(sessions, switches);
    assert(focus.size() == 1);
    const auto metrics = engine.dailyMetrics(0, sessions, switches, {});
    assert(metrics.focus_session_count == 1);
    assert(metrics.focused_ms >= 44 * minute);
    assert(metrics.context_switch_count == 0);
}

void testShortGapAndGlancesPreserveFocus() {
    AnalyticsConfig config;
    config.focus_threshold_ms = 20 * minute;
    config.max_focus_switches = 1;
    AnalyticsEngine engine(config);

    const auto gapped = engine.focusSessions(
        {
            makeSession("a", "Cursor", "Coding", 0, 15 * minute, true),
            makeSession("b", "Code", "Coding", 15 * minute + 90 * 1000, 15 * minute, true),
        },
        {});
    assert(gapped.size() == 1);
    assert(gapped[0].active_duration_ms == 30 * minute);

    std::vector<Session> chromeGlance{
        makeSession("c1", "Cursor", "Coding", 0, 20 * minute, true),
        makeSession("ch", "Google Chrome", "General", 20 * minute, 90 * 1000, false),
        makeSession("c2", "Cursor", "Coding", 20 * minute + 90 * 1000, 20 * minute, true),
    };
    std::vector<ContextSwitch> chromeSwitches{
        {"Cursor", "Google Chrome", 20 * minute},
        {"Google Chrome", "Cursor", 20 * minute + 90 * 1000},
    };
    const auto afterChrome = engine.focusSessions(chromeGlance, chromeSwitches);
    assert(afterChrome.size() == 1);
    assert(afterChrome[0].active_duration_ms == 40 * minute);
    const auto chromeMetrics = engine.dailyMetrics(0, chromeGlance, chromeSwitches, {});
    assert(chromeMetrics.context_switch_count == 0);
    assert(chromeMetrics.focused_ms == 40 * minute);

    std::vector<Session> slackGlance{
        makeSession("s1", "Cursor", "Coding", 0, 25 * minute, true),
        makeSession("sl", "Slack", "Communication", 25 * minute, 90 * 1000, false),
        makeSession("s2", "Cursor", "Coding", 25 * minute + 90 * 1000, 20 * minute, true),
    };
    std::vector<ContextSwitch> slackSwitches{
        {"Cursor", "Slack", 25 * minute},
        {"Slack", "Cursor", 25 * minute + 90 * 1000},
    };
    const auto afterSlack = engine.focusSessions(slackGlance, slackSwitches);
    assert(afterSlack.size() == 1);
    assert(afterSlack[0].active_duration_ms == 45 * minute);

    std::vector<Session> longChrome{
        makeSession("l1", "Cursor", "Coding", 0, 20 * minute, true),
        makeSession("yt", "Google Chrome", "General", 20 * minute, 5 * minute, false),
        makeSession("l2", "Cursor", "Coding", 25 * minute, 20 * minute, true),
    };
    assert(engine.focusSessions(longChrome, {}).size() == 2);

    std::vector<Session> video{
        makeSession("v1", "Cursor", "Coding", 0, 20 * minute, true),
        makeSession("vid", "YouTube", "Entertainment", 20 * minute, 90 * 1000, false, true),
        makeSession("v2", "Cursor", "Coding", 20 * minute + 90 * 1000, 20 * minute, true),
    };
    assert(engine.focusSessions(video, {}).size() == 2);
}

void testSessionStatisticsAndFocus() {
    AnalyticsConfig config;
    config.focus_threshold_ms = 20 * minute;
    config.deep_work_threshold_ms = 30 * minute;
    AnalyticsEngine engine(config);
    std::vector<Session> sessions{
        makeSession("a", "IDE", "Coding", 0, 10 * minute, true),
        makeSession("b", "IDE", "Coding", 10 * minute, 20 * minute, true),
        makeSession("c", "IDE", "Coding", 30 * minute, 40 * minute, true),
        makeSession("d", "Chat", "Communication", 70 * minute, 2 * minute, false, true),
    };
    std::vector<ContextSwitch> switches{
        {"IDE", "Browser", 35 * minute},
        {"Browser", "IDE", 36 * minute},
    };

    assert(near(engine.meanSessionMs(sessions), 18 * minute));
    assert(near(engine.medianSessionMs(sessions), 15 * minute));
    assert(engine.minMaxSessionMs(sessions) == std::make_pair(2 * minute, 40 * minute));
    assert(engine.focusSessions(sessions, switches).size() == 1);
    assert(engine.deepWorkSessions(sessions, switches).empty());
    assert(near(engine.contextSwitchesPerHour(sessions, switches), 100.0 / 60.0));
}

void testTransitionsDistractionsAndIdle() {
    AnalyticsConfig config;
    config.sustained_recovery_ms = 20 * minute;
    AnalyticsEngine engine(config);
    std::vector<Session> sessions{
        makeSession("work1", "IDE", "Coding", 0, 30 * minute, true),
        makeSession("d1", "Video", "Entertainment", 30 * minute, 5 * minute, false, true),
        makeSession("browse", "Browser", "General", 35 * minute, 3 * minute, false),
        makeSession("work2", "IDE", "Coding", 40 * minute, 25 * minute, true),
    };
    const auto episodes = engine.distractionEpisodes(sessions);
    assert(episodes.size() == 1);
    assert(episodes[0].severity == DistractionEpisode::Severity::Moderate);
    assert(episodes[0].interrupted_productive_work);
    assert(episodes[0].recovered);
    assert(episodes[0].recovery_ms == 5 * minute);
    assert(episodes[0].estimated_cost_ms == 10 * minute);

    std::vector<ContextSwitch> switches{
        {"IDE", "Video", 30 * minute},
        {"Video", "IDE", 35 * minute},
        {"IDE", "Video", 70 * minute},
        {"IDE", "IDE", 71 * minute},
    };
    const auto transitions = engine.commonTransitions(switches, 2);
    assert(transitions.size() == 2);
    assert(transitions[0] == std::make_pair(std::string("IDE -> Video"), std::size_t{2}));
    auto repeated_sessions = sessions;
    repeated_sessions.push_back(
        makeSession("work3", "IDE", "Coding", 70 * minute, 25 * minute, true));
    repeated_sessions.push_back(
        makeSession("d2", "Video", "Entertainment", 95 * minute, 3 * minute, false, true));
    const auto triggers = engine.potentialDistractionTriggers(repeated_sessions, 2);
    assert(triggers.size() == 1);
    assert(triggers[0].first == "IDE -> Video");

    const auto metrics = engine.dailyMetrics(
        0,
        sessions,
        switches,
        {{100, 200}, {150, 300}, {400, 450}, {500, 500}});
    assert(metrics.idle_ms == 250);
    assert(metrics.distraction_count == 1);
    assert(metrics.average_recovery_ms == 5 * minute);
    assert(metrics.workday_start_unix_ms == 0);
    assert(metrics.workday_end_unix_ms == 65 * minute);
    assert(metrics.workday_elapsed_ms == 65 * minute);
}

void testPeakBaselineScoreAndProfile() {
    AnalyticsEngine engine;
    std::vector<Session> sessions{
        makeSession("morning", "IDE", "Coding", 9 * hour, 45 * minute, true),
        makeSession("later", "IDE", "Coding", 14 * hour, 20 * minute, true),
    };
    assert(engine.peakProductiveHalfHour(sessions) == 18);
    AnalyticsConfig shifted_config;
    shifted_config.timezone_offset_minutes = 60;
    assert(AnalyticsEngine(shifted_config).peakProductiveHalfHour(sessions) == 20);

    std::vector<DailyMetrics> history;
    for (int index = 1; index <= 16; ++index) {
        DailyMetrics metrics;
        metrics.day_start_unix_ms = index * day;
        metrics.active_ms = 4 * hour;
        metrics.productive_ms = (2 * hour) + index * minute;
        metrics.focused_ms = 2 * hour;
        metrics.deep_work_ms = hour;
        metrics.distraction_ms = 20 * minute;
        metrics.context_switch_count = 20;
        metrics.average_session_ms = 30 * minute;
        metrics.switches_per_active_hour = 5.0;
        history.push_back(metrics);
    }
    const auto baseline = engine.baseline14Day(history, 17 * day);
    assert(baseline.focused_ms == 2 * hour);
    assert(baseline.session_count == 0);
    assert(baseline.consistency > 0.9);

    DailyMetrics current = baseline;
    current.day_start_unix_ms = 17 * day;
    current.focused_ms = 3 * hour;
    current.deep_work_ms = 2 * hour;
    current.consistency = 0.8;
    current.switches_per_active_hour = 6.0;
    const auto comparison = engine.compareToBaseline(current, baseline);
    assert(near(comparison.focused_change_percent, 50.0));

    const auto score = engine.productivityScore(current);
    assert(score.score >= 70 && score.score <= 100);
    assert(score.explanations.size() == 3);

    const auto profile = engine.workstyleProfile(history, sessions);
    assert(profile.focus_pattern == "Balanced-burst worker");
    assert(profile.context_switching == "Low");
    assert(profile.peak_half_hour == 18);
}

void testWeeklyTrendsAndInsights() {
    AnalyticsEngine engine;
    std::vector<DailyMetrics> days;
    for (int index = 0; index < 14; ++index) {
        DailyMetrics metrics;
        metrics.day_start_unix_ms = index * day;
        metrics.active_ms = 4 * hour;
        metrics.productive_ms = 3 * hour;
        metrics.focused_ms = index < 7 ? hour : 2 * hour;
        metrics.deep_work_ms = index < 7 ? 30 * minute : 90 * minute;
        metrics.distraction_ms = index < 7 ? 30 * minute : 15 * minute;
        metrics.context_switch_count = index < 7 ? 20 : 10;
        days.push_back(metrics);
    }
    const auto first_week = engine.aggregateWeek(0, days);
    assert(first_week.focused_ms == 7 * hour);
    assert(first_week.context_switch_count == 140);

    const auto trends = engine.weeklyTrends(days);
    assert(trends.size() == 2);
    assert(near(trends[1].focused_change_percent, 100.0));
    assert(near(trends[1].distraction_change_percent, -50.0));

    DailyMetrics baseline;
    baseline.focused_ms = hour;
    baseline.deep_work_ms = 30 * minute;
    baseline.distraction_ms = 20 * minute;
    baseline.context_switch_count = 20;
    DailyMetrics current = baseline;
    current.focused_ms = 90 * minute;
    current.deep_work_ms = hour;
    current.distraction_ms = 30 * minute;
    current.context_switch_count = 30;
    const auto insights = engine.insights(current, baseline);
    assert(insights.size() >= 3);
    assert(insights[0].code == "focus_above_baseline");
}

}  // namespace

int main() {
    testEmptyAndInvalidInput();
    testDistributionAndOverlapHandling();
    testProductiveSwitchPreservesFocus();
    testShortGapAndGlancesPreserveFocus();
    testSessionStatisticsAndFocus();
    testTransitionsDistractionsAndIdle();
    testPeakBaselineScoreAndProfile();
    testWeeklyTrendsAndInsights();
    std::cout << "All analytics tests passed\n";
    return 0;
}
