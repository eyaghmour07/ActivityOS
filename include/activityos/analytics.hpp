#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace activityos {

using UnixMillis = std::int64_t;

struct Session {
    std::string id;
    std::string app;
    std::string category;
    UnixMillis start_unix_ms = 0;
    UnixMillis end_unix_ms = 0;
    std::int64_t active_duration_ms = 0;
    bool productive = false;
    bool distraction = false;
};

struct ContextSwitch {
    std::string previous_app;
    std::string new_app;
    UnixMillis timestamp_unix_ms = 0;
};

struct IdlePeriod {
    UnixMillis start_unix_ms = 0;
    UnixMillis end_unix_ms = 0;
};

struct AnalyticsConfig {
    int timezone_offset_minutes = 0;
    std::int64_t focus_threshold_ms = 20 * 60 * 1000;
    std::int64_t deep_work_threshold_ms = 30 * 60 * 1000;
    std::int64_t sustained_recovery_ms = 20 * 60 * 1000;
    std::int64_t minor_distraction_max_ms = 2 * 60 * 1000;
    std::int64_t moderate_distraction_max_ms = 10 * 60 * 1000;
    std::size_t max_focus_switches = 1;
    std::size_t max_deep_work_switches = 0;
    double high_switches_per_hour = 12.0;
    double high_distraction_rate = 0.15;

    double score_focus_weight = 35.0;
    double score_deep_work_weight = 25.0;
    double score_consistency_weight = 15.0;
    double score_distraction_weight = 15.0;
    double score_switching_weight = 10.0;
    std::int64_t score_focus_target_ms = 3 * 60 * 60 * 1000;
    std::int64_t score_deep_work_target_ms = 2 * 60 * 60 * 1000;
};

struct CategoryMetrics {
    std::string category;
    std::int64_t active_ms = 0;
    double percentage = 0.0;
    bool productive = false;
    bool distraction = false;
};

struct DistractionEpisode {
    enum class Severity { Minor, Moderate, Major };

    std::string app;
    std::string category;
    UnixMillis start_unix_ms = 0;
    UnixMillis end_unix_ms = 0;
    std::int64_t duration_ms = 0;
    std::int64_t recovery_ms = 0;
    std::int64_t estimated_cost_ms = 0;
    Severity severity = Severity::Minor;
    bool interrupted_productive_work = false;
    bool recovered = false;
};

struct DailyMetrics {
    UnixMillis day_start_unix_ms = 0;
    UnixMillis workday_start_unix_ms = 0;
    UnixMillis workday_end_unix_ms = 0;
    std::int64_t workday_elapsed_ms = 0;
    std::int64_t active_ms = 0;
    std::int64_t productive_ms = 0;
    std::int64_t distraction_ms = 0;
    std::int64_t idle_ms = 0;
    std::int64_t focused_ms = 0;
    std::int64_t deep_work_ms = 0;
    std::int64_t distraction_cost_ms = 0;
    std::int64_t average_session_ms = 0;
    std::int64_t median_session_ms = 0;
    std::int64_t shortest_session_ms = 0;
    std::int64_t longest_session_ms = 0;
    std::int64_t average_recovery_ms = 0;
    std::size_t session_count = 0;
    std::size_t focus_session_count = 0;
    std::size_t deep_work_session_count = 0;
    std::size_t context_switch_count = 0;
    std::size_t distraction_count = 0;
    double switches_per_active_hour = 0.0;
    double consistency = 1.0;
    std::vector<CategoryMetrics> categories;
};

struct WorkstyleProfile {
    std::string focus_pattern;
    std::string context_switching;
    std::string distraction_sensitivity;
    std::int64_t average_sustained_session_ms = 0;
    std::int64_t average_recovery_ms = 0;
    int peak_half_hour = -1;
    std::string best_environment;
};

struct ScoreBreakdown {
    int score = 0;
    double focus_points = 0.0;
    double deep_work_points = 0.0;
    double consistency_points = 0.0;
    double distraction_penalty = 0.0;
    double switching_penalty = 0.0;
    std::vector<std::string> explanations;
};

struct TrendPoint {
    UnixMillis period_start_unix_ms = 0;
    DailyMetrics metrics;
    double focused_change_percent = 0.0;
    double deep_work_change_percent = 0.0;
    double distraction_change_percent = 0.0;
    double switching_change_percent = 0.0;
};

struct Insight {
    enum class Kind { Positive, Warning, Recommendation, Neutral };

    Kind kind = Kind::Neutral;
    std::string code;
    std::string title;
    std::string explanation;
    double magnitude_percent = 0.0;
};

class AnalyticsEngine {
public:
    explicit AnalyticsEngine(AnalyticsConfig config = {});

    const AnalyticsConfig& config() const noexcept;

    std::vector<CategoryMetrics> timeDistribution(
        const std::vector<Session>& sessions) const;
    DailyMetrics dailyMetrics(
        UnixMillis day_start_unix_ms,
        const std::vector<Session>& sessions,
        const std::vector<ContextSwitch>& switches = {},
        const std::vector<IdlePeriod>& idle_periods = {}) const;

    double meanSessionMs(const std::vector<Session>& sessions) const;
    double medianSessionMs(const std::vector<Session>& sessions) const;
    std::pair<std::int64_t, std::int64_t> minMaxSessionMs(
        const std::vector<Session>& sessions) const;

    std::vector<Session> focusSessions(
        const std::vector<Session>& sessions,
        const std::vector<ContextSwitch>& switches = {}) const;
    std::vector<Session> deepWorkSessions(
        const std::vector<Session>& sessions,
        const std::vector<ContextSwitch>& switches = {}) const;

    double contextSwitchesPerHour(
        const std::vector<Session>& sessions,
        const std::vector<ContextSwitch>& switches) const;
    std::vector<std::pair<std::string, std::size_t>> commonTransitions(
        const std::vector<ContextSwitch>& switches,
        std::size_t limit = 5) const;
    std::vector<std::pair<std::string, std::size_t>> potentialDistractionTriggers(
        const std::vector<Session>& sessions,
        std::size_t minimum_occurrences = 2) const;

    std::vector<DistractionEpisode> distractionEpisodes(
        const std::vector<Session>& sessions) const;
    int peakProductiveHalfHour(
        const std::vector<Session>& sessions) const;

    DailyMetrics baseline14Day(
        const std::vector<DailyMetrics>& history,
        UnixMillis before_day_start_unix_ms) const;
    TrendPoint compareToBaseline(
        const DailyMetrics& current,
        const DailyMetrics& baseline) const;
    double consistency(const std::vector<DailyMetrics>& days) const;
    ScoreBreakdown productivityScore(
        const DailyMetrics& metrics) const;
    WorkstyleProfile workstyleProfile(
        const std::vector<DailyMetrics>& history,
        const std::vector<Session>& sessions = {}) const;

    DailyMetrics aggregateWeek(
        UnixMillis week_start_unix_ms,
        const std::vector<DailyMetrics>& days) const;
    std::vector<TrendPoint> weeklyTrends(
        const std::vector<DailyMetrics>& days) const;
    std::vector<Insight> insights(
        const DailyMetrics& current,
        const DailyMetrics& baseline) const;

private:
    AnalyticsConfig config_;
};

}  // namespace activityos
