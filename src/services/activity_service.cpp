#include "activityos/services.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <unordered_map>

namespace activityos {
namespace {

constexpr std::int64_t kDayMs = 24LL * 60 * 60 * 1000;

bool isProductiveCategory(const std::string& category) {
    return category == "Coding" || category == "Research" ||
           category == "Planning" || category == "Administration" ||
           category == "Work";
}

double metricValue(const DailyMetrics& metrics, const std::string& metric) {
    if (metric == "active_time" || metric == "active_ms") return metrics.active_ms;
    if (metric == "focus_time" || metric == "focused_ms") return metrics.focused_ms;
    if (metric == "deep_work" || metric == "deep_work_ms") return metrics.deep_work_ms;
    if (metric == "distraction_time" || metric == "distraction_ms") {
        return metrics.distraction_ms;
    }
    if (metric == "context_switches") return metrics.context_switch_count;
    if (metric == "switch_rate") return metrics.switches_per_active_hour;
    return 0.0;
}

void writeFile(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Unable to write export: " + path.string());
    }
    output << contents;
}

} // namespace

ActivityService::ActivityService(storage::Database& database, AnalyticsConfig config)
    : database_(database), analytics_(config) {}

std::vector<Session> ActivityService::sessions(storage::DateRange range) const {
    const auto stored = database_.sessions(range);
    const auto applications = database_.applications();
    std::unordered_map<std::int64_t, storage::Application> byId;
    for (const auto& application : applications) byId[application.id] = application;

    std::vector<Session> result;
    result.reserve(stored.size());
    for (const auto& source : stored) {
        Session session;
        session.id = std::to_string(source.id);
        session.category = source.category;
        session.start_unix_ms = source.start_time;
        session.end_unix_ms = source.end_time;
        session.active_duration_ms = source.duration_ms;
        session.productive = isProductiveCategory(source.category);
        if (source.application_id && byId.count(*source.application_id) != 0) {
            const auto& application = byId.at(*source.application_id);
            session.app = application.name;
            session.distraction = application.is_distraction;
        } else {
            session.app = "Unknown";
            session.distraction = source.category == "Entertainment";
        }
        result.push_back(std::move(session));
    }
    return result;
}

std::vector<ContextSwitch> ActivityService::switches(storage::DateRange range) const {
    const auto stored = database_.contextSwitches(range);
    const auto applications = database_.applications();
    std::unordered_map<std::int64_t, std::string> names;
    for (const auto& application : applications) names[application.id] = application.name;

    std::vector<ContextSwitch> result;
    result.reserve(stored.size());
    for (const auto& source : stored) {
        ContextSwitch value;
        value.previous_app =
            source.previous_application_id && names.count(*source.previous_application_id)
                ? names.at(*source.previous_application_id)
                : "Unknown";
        value.new_app = source.new_application_id && names.count(*source.new_application_id)
                            ? names.at(*source.new_application_id)
                            : "Unknown";
        value.timestamp_unix_ms = source.timestamp;
        result.push_back(std::move(value));
    }
    return result;
}

std::vector<IdlePeriod> ActivityService::idlePeriods(storage::DateRange range) const {
    const auto stored = database_.idlePeriods(range);
    std::vector<IdlePeriod> result;
    result.reserve(stored.size());
    for (const auto& source : stored) {
        result.push_back({source.start_time, source.end_time});
    }
    return result;
}

DashboardSnapshot ActivityService::dashboard(storage::DateRange range,
                                             std::int64_t day_start_ms) const {
    const auto daySessions = sessions(range);
    const auto daySwitches = switches(range);
    const auto dayIdle = idlePeriods(range);

    DashboardSnapshot snapshot;
    snapshot.metrics =
        analytics_.dailyMetrics(day_start_ms, daySessions, daySwitches, dayIdle);

    const storage::DateRange historyRange{day_start_ms - 14 * kDayMs, day_start_ms};
    const auto history = dailyHistory(historyRange);
    snapshot.baseline = analytics_.baseline14Day(history, day_start_ms);
    snapshot.score = analytics_.productivityScore(snapshot.metrics);
    snapshot.distractions = analytics_.distractionEpisodes(daySessions);
    snapshot.transitions = analytics_.commonTransitions(daySwitches);
    snapshot.potential_triggers =
        analytics_.potentialDistractionTriggers(daySessions);

    auto profileHistory = history;
    profileHistory.push_back(snapshot.metrics);
    snapshot.profile = analytics_.workstyleProfile(profileHistory, daySessions);
    snapshot.insights = analytics_.insights(snapshot.metrics, snapshot.baseline);
    return snapshot;
}

std::vector<DailyMetrics> ActivityService::dailyHistory(storage::DateRange range) const {
    std::vector<DailyMetrics> result;
    if (range.end <= range.start) return result;
    for (auto start = range.start; start < range.end; start += kDayMs) {
        const storage::DateRange day{start, std::min(start + kDayMs, range.end)};
        result.push_back(analytics_.dailyMetrics(start, sessions(day), switches(day),
                                                 idlePeriods(day)));
    }
    return result;
}

std::vector<TrendPoint> ActivityService::weeklyTrends(storage::DateRange range) const {
    return analytics_.weeklyTrends(dailyHistory(range));
}

std::vector<storage::Application> ActivityService::applications() const {
    return database_.applications();
}

std::vector<storage::ClassificationRule> ActivityService::rules() const {
    return database_.classificationRules();
}

std::int64_t ActivityService::saveRule(const storage::ClassificationRule& rule) {
    return database_.saveClassificationRule(rule);
}

void ActivityService::deleteRule(std::int64_t id) {
    database_.removeClassificationRule(id);
}

std::vector<GoalProgress> ActivityService::goalProgress(storage::DateRange range) const {
    const auto current =
        analytics_.aggregateWeek(range.start, dailyHistory(range));
    std::vector<GoalProgress> result;
    for (const auto& goal : database_.goals(false)) {
        GoalProgress progress;
        progress.goal = goal;
        progress.current_value = metricValue(current, goal.metric);
        const bool maximum = goal.metric.rfind("max_", 0) == 0 ||
                             goal.metric == "distraction_time" ||
                             goal.metric == "context_switches" ||
                             goal.metric == "switch_rate";
        if (goal.target_value <= 0.0) {
            progress.progress = 0.0;
            progress.achieved = false;
        } else if (maximum) {
            progress.progress = progress.current_value <= goal.target_value
                                    ? 1.0
                                    : goal.target_value / progress.current_value;
            progress.achieved = progress.current_value <= goal.target_value;
        } else {
            progress.progress = std::min(1.0, progress.current_value / goal.target_value);
            progress.achieved = progress.current_value >= goal.target_value;
        }
        result.push_back(std::move(progress));
    }
    return result;
}

std::int64_t ActivityService::saveGoal(const storage::Goal& goal) {
    return database_.saveGoal(goal);
}

void ActivityService::deleteGoal(std::int64_t id) {
    database_.removeGoal(id);
}

std::vector<ExperimentResult> ActivityService::experimentResults() const {
    std::vector<ExperimentResult> result;
    for (const auto& experiment : database_.experiments()) {
        ExperimentResult value;
        value.experiment = experiment;
        if (!value.experiment.baseline_value || !value.experiment.result_value) {
            const auto measuredEnd =
                value.experiment.end_time.value_or(unixMillisecondsNow());
            const auto measuredDuration =
                std::max<std::int64_t>(0, measuredEnd - value.experiment.start_time);
            if (measuredDuration > 0) {
                const storage::DateRange baselineRange{
                    value.experiment.start_time - measuredDuration,
                    value.experiment.start_time};
                const storage::DateRange interventionRange{
                    value.experiment.start_time, measuredEnd};
                const auto totalFocus = [this](storage::DateRange range) {
                    std::int64_t focused = 0;
                    for (const auto& day : dailyHistory(range)) focused += day.focused_ms;
                    return static_cast<double>(focused);
                };
                value.experiment.baseline_value = totalFocus(baselineRange);
                value.experiment.result_value = totalFocus(interventionRange);
            }
        }
        if (value.experiment.baseline_value && value.experiment.result_value &&
            std::abs(*value.experiment.baseline_value) > 0.000001) {
            value.change_percent =
                ((*value.experiment.result_value - *value.experiment.baseline_value) /
                 std::abs(*value.experiment.baseline_value)) *
                100.0;
            value.interpretation =
                "Focused time changed by " + std::to_string(value.change_percent) +
                "% versus an equally long baseline. This is an association and does not "
                "establish causation.";
        } else {
            value.interpretation = "Collect baseline and intervention data to see a result.";
        }
        result.push_back(std::move(value));
    }
    return result;
}

std::int64_t ActivityService::saveExperiment(const storage::Experiment& experiment) {
    return database_.saveExperiment(experiment);
}

void ActivityService::deleteExperiment(std::int64_t id) {
    database_.removeExperiment(id);
}

void ActivityService::exportCsv(storage::DateRange range,
                                const std::filesystem::path& path) const {
    writeFile(path, database_.exportCsv(range));
}

void ActivityService::exportJson(storage::DateRange range,
                                 const std::filesystem::path& path) const {
    writeFile(path, database_.exportJson(range));
}

void ActivityService::deleteRange(storage::DateRange range) {
    database_.deleteRange(range);
}

void ActivityService::deleteAllActivity() {
    database_.deleteAllActivity();
}

} // namespace activityos
