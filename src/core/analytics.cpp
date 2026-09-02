#include "activityos/analytics.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <sstream>
#include <unordered_map>

namespace activityos {
namespace {

constexpr std::int64_t kHourMs = 60 * 60 * 1000;
constexpr std::int64_t kDayMs = 24 * kHourMs;
constexpr std::int64_t kWeekMs = 7 * kDayMs;

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

std::int64_t sessionDuration(const Session& session) {
    if (session.end_unix_ms <= session.start_unix_ms) {
        return 0;
    }
    const auto elapsed = session.end_unix_ms - session.start_unix_ms;
    if (session.active_duration_ms <= 0) {
        return elapsed;
    }
    return std::min(session.active_duration_ms, elapsed);
}

std::vector<Session> normalizedSessions(const std::vector<Session>& input) {
    std::vector<Session> sorted;
    sorted.reserve(input.size());
    for (const auto& session : input) {
        if (session.end_unix_ms > session.start_unix_ms) {
            sorted.push_back(session);
        }
    }
    std::stable_sort(sorted.begin(), sorted.end(), [](const Session& left, const Session& right) {
        if (left.start_unix_ms != right.start_unix_ms) {
            return left.start_unix_ms < right.start_unix_ms;
        }
        if (left.end_unix_ms != right.end_unix_ms) {
            return left.end_unix_ms < right.end_unix_ms;
        }
        if (left.id != right.id) {
            return left.id < right.id;
        }
        return left.app < right.app;
    });

    std::vector<Session> result;
    result.reserve(sorted.size());
    UnixMillis claimed_until = std::numeric_limits<UnixMillis>::min();
    for (auto session : sorted) {
        const auto original_start = session.start_unix_ms;
        const auto original_span = session.end_unix_ms - original_start;
        const auto original_active = sessionDuration(session);
        session.start_unix_ms = std::max(session.start_unix_ms, claimed_until);
        if (session.start_unix_ms >= session.end_unix_ms) {
            continue;
        }
        const auto retained_span = session.end_unix_ms - session.start_unix_ms;
        session.active_duration_ms = static_cast<std::int64_t>(
            (static_cast<long double>(original_active) * retained_span) / original_span);
        claimed_until = session.end_unix_ms;
        result.push_back(std::move(session));
    }
    return result;
}

std::vector<std::int64_t> durations(const std::vector<Session>& sessions) {
    std::vector<std::int64_t> values;
    for (const auto& session : normalizedSessions(sessions)) {
        const auto duration = sessionDuration(session);
        if (duration > 0) {
            values.push_back(duration);
        }
    }
    return values;
}

std::size_t switchesDuring(
    const Session& session,
    const std::vector<ContextSwitch>& switches) {
    return static_cast<std::size_t>(std::count_if(
        switches.begin(), switches.end(), [&session](const ContextSwitch& item) {
            return item.previous_app != item.new_app &&
                   item.timestamp_unix_ms >= session.start_unix_ms &&
                   item.timestamp_unix_ms < session.end_unix_ms;
        }));
}

double percentChange(double current, double baseline) {
    if (baseline == 0.0) {
        return current == 0.0 ? 0.0 : 100.0;
    }
    return ((current - baseline) / baseline) * 100.0;
}

std::int64_t averageInt(std::int64_t total, std::size_t count) {
    return count == 0 ? 0 : total / static_cast<std::int64_t>(count);
}

UnixMillis floorPeriod(UnixMillis timestamp, std::int64_t period) {
    auto quotient = timestamp / period;
    if (timestamp < 0 && timestamp % period != 0) {
        --quotient;
    }
    return quotient * period;
}

template <typename Value>
std::string describePercent(const std::string& label, Value value) {
    std::ostringstream stream;
    stream << label << ' ' << static_cast<int>(std::round(std::abs(value))) << "%.";
    return stream.str();
}

}  // namespace

AnalyticsEngine::AnalyticsEngine(AnalyticsConfig config) : config_(std::move(config)) {
    config_.timezone_offset_minutes =
        std::clamp(config_.timezone_offset_minutes, -24 * 60, 24 * 60);
    config_.focus_threshold_ms = std::max<std::int64_t>(0, config_.focus_threshold_ms);
    config_.deep_work_threshold_ms =
        std::max(config_.focus_threshold_ms, config_.deep_work_threshold_ms);
    config_.sustained_recovery_ms =
        std::max<std::int64_t>(0, config_.sustained_recovery_ms);
    config_.minor_distraction_max_ms =
        std::max<std::int64_t>(0, config_.minor_distraction_max_ms);
    config_.moderate_distraction_max_ms =
        std::max(config_.minor_distraction_max_ms, config_.moderate_distraction_max_ms);
    config_.high_switches_per_hour = std::max(0.0, config_.high_switches_per_hour);
    config_.high_distraction_rate = clamp01(config_.high_distraction_rate);
    config_.score_focus_weight = std::max(0.0, config_.score_focus_weight);
    config_.score_deep_work_weight = std::max(0.0, config_.score_deep_work_weight);
    config_.score_consistency_weight = std::max(0.0, config_.score_consistency_weight);
    config_.score_distraction_weight = std::max(0.0, config_.score_distraction_weight);
    config_.score_switching_weight = std::max(0.0, config_.score_switching_weight);
    config_.score_focus_target_ms = std::max<std::int64_t>(1, config_.score_focus_target_ms);
    config_.score_deep_work_target_ms =
        std::max<std::int64_t>(1, config_.score_deep_work_target_ms);
}

const AnalyticsConfig& AnalyticsEngine::config() const noexcept {
    return config_;
}

std::vector<CategoryMetrics> AnalyticsEngine::timeDistribution(
    const std::vector<Session>& sessions) const {
    struct Accumulator {
        std::int64_t active_ms = 0;
        bool productive = false;
        bool distraction = false;
    };
    std::map<std::string, Accumulator> totals;
    std::int64_t grand_total = 0;
    for (const auto& session : normalizedSessions(sessions)) {
        const auto active = sessionDuration(session);
        if (active <= 0) {
            continue;
        }
        auto& entry = totals[session.category.empty() ? "Other" : session.category];
        entry.active_ms += active;
        entry.productive = entry.productive || session.productive;
        entry.distraction = entry.distraction || session.distraction;
        grand_total += active;
    }

    std::vector<CategoryMetrics> result;
    result.reserve(totals.size());
    for (const auto& [category, value] : totals) {
        result.push_back({
            category,
            value.active_ms,
            grand_total == 0 ? 0.0
                             : (100.0 * static_cast<double>(value.active_ms) / grand_total),
            value.productive,
            value.distraction,
        });
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        if (left.active_ms != right.active_ms) {
            return left.active_ms > right.active_ms;
        }
        return left.category < right.category;
    });
    return result;
}

DailyMetrics AnalyticsEngine::dailyMetrics(
    UnixMillis day_start_unix_ms,
    const std::vector<Session>& sessions,
    const std::vector<ContextSwitch>& switches,
    const std::vector<IdlePeriod>& idle_periods) const {
    DailyMetrics result;
    result.day_start_unix_ms = day_start_unix_ms;
    const auto normalized = normalizedSessions(sessions);
    result.categories = timeDistribution(normalized);
    result.session_count = normalized.size();
    if (!normalized.empty()) {
        result.workday_start_unix_ms = normalized.front().start_unix_ms;
        result.workday_end_unix_ms = normalized.front().end_unix_ms;
        for (const auto& session : normalized) {
            result.workday_start_unix_ms =
                std::min(result.workday_start_unix_ms, session.start_unix_ms);
            result.workday_end_unix_ms =
                std::max(result.workday_end_unix_ms, session.end_unix_ms);
        }
        result.workday_elapsed_ms =
            std::max<std::int64_t>(0, result.workday_end_unix_ms -
                                         result.workday_start_unix_ms);
    }

    std::vector<std::int64_t> session_durations;
    for (const auto& session : normalized) {
        const auto active = sessionDuration(session);
        if (active <= 0) {
            continue;
        }
        result.active_ms += active;
        result.productive_ms += session.productive && !session.distraction ? active : 0;
        result.distraction_ms += session.distraction ? active : 0;
        session_durations.push_back(active);
    }
    if (!session_durations.empty()) {
        const auto total = std::accumulate(
            session_durations.begin(), session_durations.end(), std::int64_t{0});
        result.average_session_ms = averageInt(total, session_durations.size());
        std::sort(session_durations.begin(), session_durations.end());
        result.shortest_session_ms = session_durations.front();
        result.longest_session_ms = session_durations.back();
        const auto middle = session_durations.size() / 2;
        result.median_session_ms =
            session_durations.size() % 2 == 1
                ? session_durations[middle]
                : session_durations[middle - 1] / 2 + session_durations[middle] / 2 +
                      ((session_durations[middle - 1] % 2 +
                        session_durations[middle] % 2) /
                       2);
    }

    const auto focus = focusSessions(normalized, switches);
    const auto deep = deepWorkSessions(normalized, switches);
    result.focus_session_count = focus.size();
    result.deep_work_session_count = deep.size();
    for (const auto& session : focus) {
        result.focused_ms += sessionDuration(session);
    }
    for (const auto& session : deep) {
        result.deep_work_ms += sessionDuration(session);
    }

    result.context_switch_count = static_cast<std::size_t>(std::count_if(
        switches.begin(), switches.end(), [](const ContextSwitch& item) {
            return item.previous_app != item.new_app;
        }));
    result.switches_per_active_hour =
        contextSwitchesPerHour(normalized, switches);

    const auto distractions = distractionEpisodes(normalized);
    result.distraction_count = distractions.size();
    std::int64_t total_recovery = 0;
    std::size_t recovered_count = 0;
    for (const auto& episode : distractions) {
        result.distraction_cost_ms += episode.estimated_cost_ms;
        if (episode.recovered) {
            total_recovery += episode.recovery_ms;
            ++recovered_count;
        }
    }
    result.average_recovery_ms = averageInt(total_recovery, recovered_count);

    std::vector<std::pair<UnixMillis, UnixMillis>> valid_idle;
    for (const auto& idle : idle_periods) {
        if (idle.end_unix_ms > idle.start_unix_ms) {
            valid_idle.emplace_back(idle.start_unix_ms, idle.end_unix_ms);
        }
    }
    std::sort(valid_idle.begin(), valid_idle.end());
    UnixMillis idle_start = 0;
    UnixMillis idle_end = 0;
    bool has_idle = false;
    for (const auto& interval : valid_idle) {
        if (!has_idle || interval.first > idle_end) {
            if (has_idle) {
                result.idle_ms += idle_end - idle_start;
            }
            idle_start = interval.first;
            idle_end = interval.second;
            has_idle = true;
        } else {
            idle_end = std::max(idle_end, interval.second);
        }
    }
    if (has_idle) {
        result.idle_ms += idle_end - idle_start;
    }
    return result;
}

double AnalyticsEngine::meanSessionMs(const std::vector<Session>& sessions) const {
    const auto values = durations(sessions);
    if (values.empty()) {
        return 0.0;
    }
    return static_cast<double>(
               std::accumulate(values.begin(), values.end(), std::int64_t{0})) /
           values.size();
}

double AnalyticsEngine::medianSessionMs(const std::vector<Session>& sessions) const {
    auto values = durations(sessions);
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    if (values.size() % 2 == 1) {
        return static_cast<double>(values[middle]);
    }
    return static_cast<double>(values[middle - 1]) / 2.0 +
           static_cast<double>(values[middle]) / 2.0;
}

std::pair<std::int64_t, std::int64_t> AnalyticsEngine::minMaxSessionMs(
    const std::vector<Session>& sessions) const {
    const auto values = durations(sessions);
    if (values.empty()) {
        return {0, 0};
    }
    const auto [minimum, maximum] = std::minmax_element(values.begin(), values.end());
    return {*minimum, *maximum};
}

std::vector<Session> AnalyticsEngine::focusSessions(
    const std::vector<Session>& sessions,
    const std::vector<ContextSwitch>& switches) const {
    std::vector<Session> result;
    for (const auto& session : normalizedSessions(sessions)) {
        if (session.productive && !session.distraction &&
            sessionDuration(session) >= config_.focus_threshold_ms &&
            switchesDuring(session, switches) <= config_.max_focus_switches) {
            result.push_back(session);
        }
    }
    return result;
}

std::vector<Session> AnalyticsEngine::deepWorkSessions(
    const std::vector<Session>& sessions,
    const std::vector<ContextSwitch>& switches) const {
    std::vector<Session> result;
    for (const auto& session : normalizedSessions(sessions)) {
        if (session.productive && !session.distraction &&
            sessionDuration(session) >= config_.deep_work_threshold_ms &&
            switchesDuring(session, switches) <= config_.max_deep_work_switches) {
            result.push_back(session);
        }
    }
    return result;
}

double AnalyticsEngine::contextSwitchesPerHour(
    const std::vector<Session>& sessions,
    const std::vector<ContextSwitch>& switches) const {
    std::int64_t active_ms = 0;
    for (const auto& session : normalizedSessions(sessions)) {
        active_ms += sessionDuration(session);
    }
    if (active_ms <= 0) {
        return 0.0;
    }
    const auto valid_switches = std::count_if(
        switches.begin(), switches.end(), [](const ContextSwitch& item) {
            return item.previous_app != item.new_app;
        });
    return static_cast<double>(valid_switches) * kHourMs /
           static_cast<double>(active_ms);
}

std::vector<std::pair<std::string, std::size_t>> AnalyticsEngine::commonTransitions(
    const std::vector<ContextSwitch>& switches,
    std::size_t limit) const {
    std::map<std::string, std::size_t> counts;
    for (const auto& item : switches) {
        if (item.previous_app != item.new_app) {
            ++counts[item.previous_app + " -> " + item.new_app];
        }
    }
    std::vector<std::pair<std::string, std::size_t>> result(
        counts.begin(), counts.end());
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        if (left.second != right.second) {
            return left.second > right.second;
        }
        return left.first < right.first;
    });
    if (result.size() > limit) {
        result.resize(limit);
    }
    return result;
}

std::vector<std::pair<std::string, std::size_t>>
AnalyticsEngine::potentialDistractionTriggers(
    const std::vector<Session>& sessions,
    std::size_t minimum_occurrences) const {
    const auto normalized = normalizedSessions(sessions);
    std::map<std::string, std::size_t> counts;
    for (std::size_t index = 1; index < normalized.size(); ++index) {
        const auto& previous = normalized[index - 1];
        const auto& current = normalized[index];
        if (previous.productive && !previous.distraction && current.distraction) {
            ++counts[previous.app + " -> " + current.app];
        }
    }
    std::vector<std::pair<std::string, std::size_t>> result;
    for (const auto& entry : counts) {
        if (entry.second >= minimum_occurrences) result.push_back(entry);
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        if (left.second != right.second) return left.second > right.second;
        return left.first < right.first;
    });
    return result;
}

std::vector<DistractionEpisode> AnalyticsEngine::distractionEpisodes(
    const std::vector<Session>& sessions) const {
    const auto normalized = normalizedSessions(sessions);
    std::vector<DistractionEpisode> result;
    for (std::size_t index = 0; index < normalized.size(); ++index) {
        const auto& session = normalized[index];
        if (!session.distraction) {
            continue;
        }
        DistractionEpisode episode;
        episode.app = session.app;
        episode.category = session.category;
        episode.start_unix_ms = session.start_unix_ms;
        episode.end_unix_ms = session.end_unix_ms;
        episode.duration_ms = sessionDuration(session);
        episode.interrupted_productive_work =
            index > 0 && normalized[index - 1].productive &&
            !normalized[index - 1].distraction;
        if (episode.duration_ms < config_.minor_distraction_max_ms) {
            episode.severity = DistractionEpisode::Severity::Minor;
        } else if (episode.duration_ms <= config_.moderate_distraction_max_ms) {
            episode.severity = DistractionEpisode::Severity::Moderate;
        } else {
            episode.severity = DistractionEpisode::Severity::Major;
        }

        for (std::size_t next = index + 1; next < normalized.size(); ++next) {
            const auto& candidate = normalized[next];
            if (candidate.productive && !candidate.distraction &&
                sessionDuration(candidate) >= config_.sustained_recovery_ms) {
                episode.recovery_ms =
                    std::max<std::int64_t>(0, candidate.start_unix_ms - session.end_unix_ms);
                episode.recovered = true;
                break;
            }
        }
        episode.estimated_cost_ms = episode.duration_ms + episode.recovery_ms;
        result.push_back(std::move(episode));
    }
    return result;
}

int AnalyticsEngine::peakProductiveHalfHour(
    const std::vector<Session>& sessions) const {
    std::array<long double, 48> totals{};
    for (const auto& session : normalizedSessions(sessions)) {
        if (!session.productive || session.distraction) {
            continue;
        }
        const auto active = sessionDuration(session);
        const auto span = session.end_unix_ms - session.start_unix_ms;
        if (active <= 0 || span <= 0) {
            continue;
        }
        auto cursor = session.start_unix_ms;
        const auto offset_ms =
            static_cast<std::int64_t>(config_.timezone_offset_minutes) * 60 * 1000;
        while (cursor < session.end_unix_ms) {
            const auto local_cursor = cursor + offset_ms;
            const auto local_day_start = floorPeriod(local_cursor, kDayMs);
            const auto slot =
                static_cast<int>((local_cursor - local_day_start) / (30 * 60 * 1000));
            const auto slot_end =
                local_day_start +
                static_cast<std::int64_t>(slot + 1) * 30 * 60 * 1000 - offset_ms;
            const auto overlap_end = std::min(session.end_unix_ms, slot_end);
            totals[static_cast<std::size_t>(slot)] +=
                static_cast<long double>(active) * (overlap_end - cursor) / span;
            cursor = overlap_end;
        }
    }
    const auto maximum = std::max_element(totals.begin(), totals.end());
    if (maximum == totals.end() || *maximum <= 0.0L) {
        return -1;
    }
    return static_cast<int>(std::distance(totals.begin(), maximum));
}

DailyMetrics AnalyticsEngine::baseline14Day(
    const std::vector<DailyMetrics>& history,
    UnixMillis before_day_start_unix_ms) const {
    std::vector<DailyMetrics> eligible;
    for (const auto& day : history) {
        if (day.day_start_unix_ms < before_day_start_unix_ms) {
            eligible.push_back(day);
        }
    }
    std::sort(eligible.begin(), eligible.end(), [](const auto& left, const auto& right) {
        return left.day_start_unix_ms > right.day_start_unix_ms;
    });
    if (eligible.size() > 14) {
        eligible.resize(14);
    }
    DailyMetrics result;
    result.day_start_unix_ms = before_day_start_unix_ms;
    if (eligible.empty()) {
        return result;
    }

    std::map<std::string, CategoryMetrics> category_totals;
    for (const auto& day : eligible) {
        result.active_ms += day.active_ms;
        result.productive_ms += day.productive_ms;
        result.distraction_ms += day.distraction_ms;
        result.idle_ms += day.idle_ms;
        result.focused_ms += day.focused_ms;
        result.deep_work_ms += day.deep_work_ms;
        result.distraction_cost_ms += day.distraction_cost_ms;
        result.average_session_ms += day.average_session_ms;
        result.median_session_ms += day.median_session_ms;
        result.shortest_session_ms += day.shortest_session_ms;
        result.longest_session_ms += day.longest_session_ms;
        result.average_recovery_ms += day.average_recovery_ms;
        result.session_count += day.session_count;
        result.focus_session_count += day.focus_session_count;
        result.deep_work_session_count += day.deep_work_session_count;
        result.context_switch_count += day.context_switch_count;
        result.distraction_count += day.distraction_count;
        result.switches_per_active_hour += day.switches_per_active_hour;
        for (const auto& category : day.categories) {
            auto& total = category_totals[category.category];
            total.category = category.category;
            total.active_ms += category.active_ms;
            total.productive = total.productive || category.productive;
            total.distraction = total.distraction || category.distraction;
        }
    }
    const auto count = eligible.size();
    const auto divide = [count](std::int64_t& value) {
        value /= static_cast<std::int64_t>(count);
    };
    divide(result.active_ms);
    divide(result.productive_ms);
    divide(result.distraction_ms);
    divide(result.idle_ms);
    divide(result.focused_ms);
    divide(result.deep_work_ms);
    divide(result.distraction_cost_ms);
    divide(result.average_session_ms);
    divide(result.median_session_ms);
    divide(result.shortest_session_ms);
    divide(result.longest_session_ms);
    divide(result.average_recovery_ms);
    result.session_count /= count;
    result.focus_session_count /= count;
    result.deep_work_session_count /= count;
    result.context_switch_count /= count;
    result.distraction_count /= count;
    result.switches_per_active_hour /= static_cast<double>(count);
    result.consistency = consistency(eligible);

    for (auto& [name, category] : category_totals) {
        category.active_ms /= static_cast<std::int64_t>(count);
        category.percentage =
            result.active_ms == 0
                ? 0.0
                : 100.0 * static_cast<double>(category.active_ms) / result.active_ms;
        result.categories.push_back(category);
    }
    std::sort(result.categories.begin(), result.categories.end(), [](const auto& a, const auto& b) {
        if (a.active_ms != b.active_ms) {
            return a.active_ms > b.active_ms;
        }
        return a.category < b.category;
    });
    return result;
}

TrendPoint AnalyticsEngine::compareToBaseline(
    const DailyMetrics& current,
    const DailyMetrics& baseline) const {
    TrendPoint result;
    result.period_start_unix_ms = current.day_start_unix_ms;
    result.metrics = current;
    result.focused_change_percent =
        percentChange(current.focused_ms, baseline.focused_ms);
    result.deep_work_change_percent =
        percentChange(current.deep_work_ms, baseline.deep_work_ms);
    result.distraction_change_percent =
        percentChange(current.distraction_ms, baseline.distraction_ms);
    result.switching_change_percent =
        percentChange(current.context_switch_count, baseline.context_switch_count);
    return result;
}

double AnalyticsEngine::consistency(const std::vector<DailyMetrics>& days) const {
    if (days.empty()) {
        return 1.0;
    }
    const double mean = std::accumulate(
                            days.begin(), days.end(), 0.0,
                            [](double total, const DailyMetrics& day) {
                                return total + static_cast<double>(day.productive_ms);
                            }) /
                        days.size();
    if (mean <= 0.0) {
        return std::all_of(days.begin(), days.end(), [](const DailyMetrics& day) {
                   return day.productive_ms == 0;
               })
                   ? 1.0
                   : 0.0;
    }
    double square_sum = 0.0;
    for (const auto& day : days) {
        const auto difference = static_cast<double>(day.productive_ms) - mean;
        square_sum += difference * difference;
    }
    const auto standard_deviation = std::sqrt(square_sum / days.size());
    return clamp01(1.0 - standard_deviation / mean);
}

ScoreBreakdown AnalyticsEngine::productivityScore(
    const DailyMetrics& metrics) const {
    ScoreBreakdown result;
    result.focus_points = config_.score_focus_weight *
                          clamp01(static_cast<double>(metrics.focused_ms) /
                                  config_.score_focus_target_ms);
    result.deep_work_points = config_.score_deep_work_weight *
                              clamp01(static_cast<double>(metrics.deep_work_ms) /
                                      config_.score_deep_work_target_ms);
    result.consistency_points =
        config_.score_consistency_weight * clamp01(metrics.consistency);

    const auto distraction_rate =
        metrics.active_ms <= 0
            ? 0.0
            : static_cast<double>(metrics.distraction_ms) / metrics.active_ms;
    result.distraction_penalty =
        config_.score_distraction_weight * clamp01(distraction_rate);
    const auto switching_ratio =
        config_.high_switches_per_hour <= 0.0
            ? (metrics.switches_per_active_hour > 0.0 ? 1.0 : 0.0)
            : metrics.switches_per_active_hour / config_.high_switches_per_hour;
    result.switching_penalty =
        config_.score_switching_weight * clamp01(switching_ratio);

    const auto raw_score =
        result.focus_points + result.deep_work_points + result.consistency_points +
        config_.score_distraction_weight + config_.score_switching_weight -
        result.distraction_penalty - result.switching_penalty;
    result.score = static_cast<int>(std::round(std::clamp(raw_score, 0.0, 100.0)));

    std::ostringstream focus;
    focus << "Focus contributed " << static_cast<int>(std::round(result.focus_points))
          << " points.";
    result.explanations.push_back(focus.str());
    std::ostringstream deep;
    deep << "Deep work contributed " << static_cast<int>(std::round(result.deep_work_points))
         << " points.";
    result.explanations.push_back(deep.str());
    std::ostringstream penalties;
    penalties << "Distractions and switching deducted "
              << static_cast<int>(std::round(
                     result.distraction_penalty + result.switching_penalty))
              << " points.";
    result.explanations.push_back(penalties.str());
    return result;
}

WorkstyleProfile AnalyticsEngine::workstyleProfile(
    const std::vector<DailyMetrics>& history,
    const std::vector<Session>& sessions) const {
    WorkstyleProfile result;
    std::int64_t total_average_session = 0;
    std::int64_t total_recovery = 0;
    std::size_t recovery_days = 0;
    double total_switch_rate = 0.0;
    std::int64_t total_active = 0;
    std::int64_t total_distraction = 0;
    for (const auto& day : history) {
        total_average_session += day.average_session_ms;
        total_switch_rate += day.switches_per_active_hour;
        total_active += day.active_ms;
        total_distraction += day.distraction_ms;
        if (day.average_recovery_ms > 0) {
            total_recovery += day.average_recovery_ms;
            ++recovery_days;
        }
    }
    result.average_sustained_session_ms =
        averageInt(total_average_session, history.size());
    result.average_recovery_ms = averageInt(total_recovery, recovery_days);
    const auto average_switch_rate =
        history.empty() ? 0.0 : total_switch_rate / history.size();
    const auto distraction_rate =
        total_active == 0 ? 0.0 : static_cast<double>(total_distraction) / total_active;

    if (result.average_sustained_session_ms >= 45 * 60 * 1000) {
        result.focus_pattern = "Long-burst worker";
    } else if (result.average_sustained_session_ms >= 20 * 60 * 1000) {
        result.focus_pattern = "Balanced-burst worker";
    } else {
        result.focus_pattern = "Short-burst worker";
    }
    if (average_switch_rate >= config_.high_switches_per_hour) {
        result.context_switching = "High";
    } else if (average_switch_rate >= config_.high_switches_per_hour * 0.5) {
        result.context_switching = "Moderate";
    } else {
        result.context_switching = "Low";
    }
    if (distraction_rate >= config_.high_distraction_rate) {
        result.distraction_sensitivity = "High";
    } else if (distraction_rate >= config_.high_distraction_rate * 0.5) {
        result.distraction_sensitivity = "Moderate";
    } else {
        result.distraction_sensitivity = "Low";
    }
    result.peak_half_hour = peakProductiveHalfHour(sessions);
    result.best_environment =
        average_switch_rate < config_.high_switches_per_hour * 0.5 &&
                distraction_rate < config_.high_distraction_rate * 0.5
            ? "Low interruption"
            : "Reduce communication and browsing";
    return result;
}

DailyMetrics AnalyticsEngine::aggregateWeek(
    UnixMillis week_start_unix_ms,
    const std::vector<DailyMetrics>& days) const {
    DailyMetrics result;
    result.day_start_unix_ms = week_start_unix_ms;
    std::vector<DailyMetrics> included;
    std::map<std::string, CategoryMetrics> category_totals;
    for (const auto& day : days) {
        if (day.day_start_unix_ms < week_start_unix_ms ||
            day.day_start_unix_ms >= week_start_unix_ms + kWeekMs) {
            continue;
        }
        included.push_back(day);
        result.active_ms += day.active_ms;
        result.productive_ms += day.productive_ms;
        result.distraction_ms += day.distraction_ms;
        result.idle_ms += day.idle_ms;
        result.focused_ms += day.focused_ms;
        result.deep_work_ms += day.deep_work_ms;
        result.distraction_cost_ms += day.distraction_cost_ms;
        result.average_session_ms += day.average_session_ms;
        result.median_session_ms += day.median_session_ms;
        result.shortest_session_ms =
            result.shortest_session_ms == 0
                ? day.shortest_session_ms
                : std::min(result.shortest_session_ms, day.shortest_session_ms);
        result.longest_session_ms =
            std::max(result.longest_session_ms, day.longest_session_ms);
        result.average_recovery_ms += day.average_recovery_ms;
        result.session_count += day.session_count;
        result.focus_session_count += day.focus_session_count;
        result.deep_work_session_count += day.deep_work_session_count;
        result.context_switch_count += day.context_switch_count;
        result.distraction_count += day.distraction_count;
        for (const auto& category : day.categories) {
            auto& total = category_totals[category.category];
            total.category = category.category;
            total.active_ms += category.active_ms;
            total.productive = total.productive || category.productive;
            total.distraction = total.distraction || category.distraction;
        }
    }
    if (!included.empty()) {
        result.average_session_ms /= static_cast<std::int64_t>(included.size());
        result.median_session_ms /= static_cast<std::int64_t>(included.size());
        result.average_recovery_ms /= static_cast<std::int64_t>(included.size());
    }
    result.switches_per_active_hour =
        result.active_ms == 0
            ? 0.0
            : static_cast<double>(result.context_switch_count) * kHourMs /
                  result.active_ms;
    result.consistency = consistency(included);
    for (auto& [name, category] : category_totals) {
        category.percentage =
            result.active_ms == 0
                ? 0.0
                : 100.0 * static_cast<double>(category.active_ms) / result.active_ms;
        result.categories.push_back(category);
    }
    std::sort(result.categories.begin(), result.categories.end(), [](const auto& a, const auto& b) {
        if (a.active_ms != b.active_ms) {
            return a.active_ms > b.active_ms;
        }
        return a.category < b.category;
    });
    return result;
}

std::vector<TrendPoint> AnalyticsEngine::weeklyTrends(
    const std::vector<DailyMetrics>& days) const {
    std::map<UnixMillis, std::vector<DailyMetrics>> grouped;
    for (const auto& day : days) {
        grouped[floorPeriod(day.day_start_unix_ms, kWeekMs)].push_back(day);
    }
    std::vector<TrendPoint> result;
    DailyMetrics previous;
    bool has_previous = false;
    for (const auto& [week_start, week_days] : grouped) {
        TrendPoint point;
        point.period_start_unix_ms = week_start;
        point.metrics = aggregateWeek(week_start, week_days);
        if (has_previous) {
            point.focused_change_percent =
                percentChange(point.metrics.focused_ms, previous.focused_ms);
            point.deep_work_change_percent =
                percentChange(point.metrics.deep_work_ms, previous.deep_work_ms);
            point.distraction_change_percent =
                percentChange(point.metrics.distraction_ms, previous.distraction_ms);
            point.switching_change_percent =
                percentChange(
                    point.metrics.context_switch_count, previous.context_switch_count);
        }
        previous = point.metrics;
        has_previous = true;
        result.push_back(std::move(point));
    }
    return result;
}

std::vector<Insight> AnalyticsEngine::insights(
    const DailyMetrics& current,
    const DailyMetrics& baseline) const {
    std::vector<Insight> result;
    const auto comparison = compareToBaseline(current, baseline);
    if (baseline.focused_ms > 0 &&
        comparison.focused_change_percent >= 20.0) {
        result.push_back({
            Insight::Kind::Positive,
            "focus_above_baseline",
            "Focused time is above your baseline",
            describePercent("Focused time increased by", comparison.focused_change_percent),
            comparison.focused_change_percent,
        });
    } else if (baseline.focused_ms > 0 &&
               comparison.focused_change_percent <= -20.0) {
        result.push_back({
            Insight::Kind::Warning,
            "focus_below_baseline",
            "Focused time is below your baseline",
            describePercent("Focused time decreased by", comparison.focused_change_percent),
            comparison.focused_change_percent,
        });
    }
    if (baseline.context_switch_count > 0 &&
        comparison.switching_change_percent >= 25.0) {
        result.push_back({
            Insight::Kind::Warning,
            "fragmentation_above_baseline",
            "Work was unusually fragmented",
            describePercent(
                "Context switching exceeded your baseline by",
                comparison.switching_change_percent),
            comparison.switching_change_percent,
        });
    }
    if (baseline.distraction_ms > 0 &&
        comparison.distraction_change_percent >= 25.0) {
        result.push_back({
            Insight::Kind::Recommendation,
            "distraction_above_baseline",
            "Distraction time was unusually high",
            describePercent(
                "Distraction time exceeded your baseline by",
                comparison.distraction_change_percent),
            comparison.distraction_change_percent,
        });
    }
    if (current.deep_work_ms > 0 && current.focused_ms > 0 &&
        static_cast<double>(current.deep_work_ms) / current.focused_ms >= 0.6) {
        result.push_back({
            Insight::Kind::Positive,
            "deep_work_share",
            "Most focused time was deep work",
            "At least 60% of focused time met the stricter deep-work rule.",
            100.0 * static_cast<double>(current.deep_work_ms) / current.focused_ms,
        });
    }
    if (result.empty()) {
        result.push_back({
            Insight::Kind::Neutral,
            "within_baseline",
            "Workstyle was near your baseline",
            "No configured rule found a material deviation today.",
            0.0,
        });
    }
    return result;
}

}  // namespace activityos
