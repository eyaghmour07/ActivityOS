#pragma once

#include "activityos/activity_source.hpp"
#include "activityos/analytics.hpp"
#include "activityos/storage.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace activityos {

struct TrackerConfig {
    std::int64_t idle_threshold_ms{5 * 60 * 1000};
    std::int64_t heartbeat_interval_ms{60 * 1000};
    std::int64_t session_merge_gap_ms{2 * 60 * 1000};
    bool persist_window_titles{false};
};

struct TrackerStatus {
    bool paused{false};
    bool idle{false};
    std::string active_application;
    std::string active_category;
    ActivitySourceStatus source_status{ActivitySourceStatus::available};
    std::optional<std::string> message;
};

class TrackerService {
public:
    TrackerService(storage::Database& database,
                   std::unique_ptr<ActivitySource> source,
                   TrackerConfig config = {});
    ~TrackerService();

    TrackerService(const TrackerService&) = delete;
    TrackerService& operator=(const TrackerService&) = delete;

    TrackerStatus poll(std::int64_t now_unix_ms);
    void pause(std::int64_t now_unix_ms);
    void resume();
    void shutdown(std::int64_t now_unix_ms);
    void updateConfig(TrackerConfig config);
    [[nodiscard]] TrackerStatus status() const;

private:
    struct CurrentSession {
        std::int64_t application_id{};
        std::string application_name;
        std::string category;
        std::int64_t start_ms{};
        std::int64_t last_seen_ms{};
    };

    storage::Database& database_;
    std::unique_ptr<ActivitySource> source_;
    TrackerConfig config_;
    TrackerStatus status_;
    std::optional<CurrentSession> current_;
    std::optional<std::int64_t> idle_started_ms_;
    std::int64_t last_event_ms_{};

    storage::Application classify(const ActivitySnapshot& snapshot,
                                  std::int64_t now_unix_ms);
    void closeCurrent(std::int64_t end_unix_ms);
    void recordEvent(std::optional<std::int64_t> application_id,
                     std::int64_t timestamp,
                     std::string type,
                     const std::optional<std::string>& title = std::nullopt);
};

struct DashboardSnapshot {
    DailyMetrics metrics;
    DailyMetrics baseline;
    ScoreBreakdown score;
    WorkstyleProfile profile;
    std::vector<DistractionEpisode> distractions;
    std::vector<Insight> insights;
    std::vector<std::pair<std::string, std::size_t>> transitions;
    std::vector<std::pair<std::string, std::size_t>> potential_triggers;
};

struct GoalProgress {
    storage::Goal goal;
    double current_value{};
    double progress{};
    bool achieved{};
};

struct ExperimentResult {
    storage::Experiment experiment;
    double change_percent{};
    std::string interpretation;
};

class ActivityService {
public:
    explicit ActivityService(storage::Database& database,
                             AnalyticsConfig config = {});

    [[nodiscard]] DashboardSnapshot dashboard(storage::DateRange range,
                                              std::int64_t day_start_ms) const;
    [[nodiscard]] std::vector<DailyMetrics> dailyHistory(storage::DateRange range) const;
    [[nodiscard]] std::vector<TrendPoint> weeklyTrends(storage::DateRange range) const;
    [[nodiscard]] std::vector<storage::Application> applications() const;
    [[nodiscard]] std::vector<storage::ClassificationRule> rules() const;
    std::int64_t saveRule(const storage::ClassificationRule& rule);
    void deleteRule(std::int64_t id);

    [[nodiscard]] std::vector<GoalProgress> goalProgress(storage::DateRange range) const;
    std::int64_t saveGoal(const storage::Goal& goal);
    void deleteGoal(std::int64_t id);

    [[nodiscard]] std::vector<ExperimentResult> experimentResults() const;
    std::int64_t saveExperiment(const storage::Experiment& experiment);
    void deleteExperiment(std::int64_t id);

    void exportCsv(storage::DateRange range, const std::filesystem::path& path) const;
    void exportJson(storage::DateRange range, const std::filesystem::path& path) const;
    void deleteRange(storage::DateRange range);
    void deleteAllActivity();

    [[nodiscard]] std::vector<Session> sessions(storage::DateRange range) const;
    [[nodiscard]] std::vector<ContextSwitch> switches(storage::DateRange range) const;

private:
    storage::Database& database_;
    AnalyticsEngine analytics_;

    [[nodiscard]] std::vector<IdlePeriod> idlePeriods(storage::DateRange range) const;
};

std::int64_t unixMillisecondsNow();

} // namespace activityos
