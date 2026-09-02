#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;

namespace activityos::storage {

using Timestamp = std::int64_t; // Unix time in milliseconds.

struct DateRange {
    Timestamp start{};
    Timestamp end{}; // Half-open: [start, end).
};

struct Application {
    std::int64_t id{};
    std::string name;
    std::string category{"Other"};
    bool is_distraction{};
    Timestamp created_at{};
};

struct ClassificationRule {
    std::int64_t id{};
    std::string application_pattern;
    std::string title_pattern;
    std::string category{"Other"};
    bool is_distraction{};
    int priority{};
    bool enabled{true};
    Timestamp created_at{};
    Timestamp updated_at{};
};

struct ActivityEvent {
    std::int64_t id{};
    std::optional<std::int64_t> application_id;
    Timestamp timestamp{};
    std::string event_type{"ACTIVE"};
    std::string window_title;
};

struct IdlePeriod {
    std::int64_t id{};
    Timestamp start_time{};
    Timestamp end_time{};
    std::int64_t duration_ms{};
};

struct Session {
    std::int64_t id{};
    std::optional<std::int64_t> application_id;
    std::string category{"Other"};
    Timestamp start_time{};
    Timestamp end_time{};
    std::int64_t duration_ms{};
    int interruption_count{};
    int context_switch_count{};
    bool is_focus{};
    bool is_deep_work{};
};

struct ContextSwitch {
    std::int64_t id{};
    std::optional<std::int64_t> previous_application_id;
    std::optional<std::int64_t> new_application_id;
    Timestamp timestamp{};
};

struct Goal {
    std::int64_t id{};
    std::string name;
    std::string metric;
    double target_value{};
    std::string period{"daily"};
    bool active{true};
    Timestamp created_at{};
    std::optional<Timestamp> completed_at;
};

struct Experiment {
    std::int64_t id{};
    std::string name;
    std::string hypothesis;
    std::string intervention;
    Timestamp start_time{};
    std::optional<Timestamp> end_time;
    std::string status{"planned"};
    std::optional<double> baseline_value;
    std::optional<double> result_value;
    std::string notes;
    Timestamp created_at{};
};

struct Insight {
    std::int64_t id{};
    std::string kind;
    std::string title;
    std::string message;
    double score{};
    Timestamp generated_at{};
    std::optional<Timestamp> period_start;
    std::optional<Timestamp> period_end;
    bool dismissed{};
};

class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;

    static Database inMemory();
    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] sqlite3* nativeHandle() noexcept;

    void migrate(std::string_view migration_sql = {});
    void beginTransaction();
    void commit();
    void rollback() noexcept;

    void setSetting(std::string_view key, std::string_view value);
    [[nodiscard]] std::optional<std::string> setting(std::string_view key) const;
    void removeSetting(std::string_view key);

    std::int64_t upsertApplication(const Application& application);
    [[nodiscard]] std::optional<Application> application(std::int64_t id) const;
    [[nodiscard]] std::optional<Application> applicationByName(std::string_view name) const;
    [[nodiscard]] std::vector<Application> applications() const;
    void removeApplication(std::int64_t id);

    std::int64_t saveClassificationRule(const ClassificationRule& rule);
    [[nodiscard]] std::vector<ClassificationRule> classificationRules() const;
    void removeClassificationRule(std::int64_t id);

    void excludeApplication(std::string_view application_name);
    void includeApplication(std::string_view application_name);
    [[nodiscard]] bool isApplicationExcluded(std::string_view application_name) const;
    [[nodiscard]] std::vector<std::string> excludedApplications() const;
    void excludeCategory(std::string_view category);
    void includeCategory(std::string_view category);
    [[nodiscard]] bool isCategoryExcluded(std::string_view category) const;
    [[nodiscard]] std::vector<std::string> excludedCategories() const;

    std::int64_t addActivityEvent(const ActivityEvent& event);
    std::int64_t addIdlePeriod(const IdlePeriod& period);
    std::int64_t addSession(const Session& session);
    std::int64_t addContextSwitch(const ContextSwitch& context_switch);

    [[nodiscard]] std::vector<ActivityEvent> activityEvents(DateRange range) const;
    [[nodiscard]] std::vector<IdlePeriod> idlePeriods(DateRange range) const;
    [[nodiscard]] std::vector<Session> sessions(DateRange range) const;
    [[nodiscard]] std::vector<ContextSwitch> contextSwitches(DateRange range) const;

    void deleteRange(DateRange range);
    void deleteAllActivity();
    std::int64_t cleanupRetention(Timestamp cutoff);

    std::int64_t saveGoal(const Goal& goal);
    [[nodiscard]] std::vector<Goal> goals(bool active_only = false) const;
    void removeGoal(std::int64_t id);

    std::int64_t saveExperiment(const Experiment& experiment);
    [[nodiscard]] std::vector<Experiment> experiments() const;
    void removeExperiment(std::int64_t id);

    std::int64_t saveInsight(const Insight& insight);
    [[nodiscard]] std::vector<Insight> insights(DateRange range) const;
    void dismissInsight(std::int64_t id, bool dismissed = true);
    void removeInsight(std::int64_t id);

    [[nodiscard]] std::string exportCsv(DateRange range) const;
    [[nodiscard]] std::string exportJson(DateRange range) const;

private:
    sqlite3* db_{};
    void execute(std::string_view sql) const;
};

class Transaction {
public:
    explicit Transaction(Database& database);
    ~Transaction();
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    void commit();
    void rollback() noexcept;

private:
    Database* database_;
    bool finished_{};
};

} // namespace activityos::storage
