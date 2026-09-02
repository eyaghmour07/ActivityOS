#include "activityos/storage.hpp"

#include <sqlite3.h>

#include <chrono>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace activityos::storage {
namespace {

constexpr std::string_view kSchema = R"sql(
CREATE TABLE IF NOT EXISTS schema_migrations(version INTEGER PRIMARY KEY, applied_at INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY, value TEXT NOT NULL, updated_at INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS applications(application_id INTEGER PRIMARY KEY, name TEXT NOT NULL COLLATE NOCASE UNIQUE, category TEXT NOT NULL DEFAULT 'Other', is_distraction INTEGER NOT NULL DEFAULT 0 CHECK(is_distraction IN(0,1)), created_at INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS classification_rules(rule_id INTEGER PRIMARY KEY, application_pattern TEXT NOT NULL, title_pattern TEXT NOT NULL DEFAULT '', category TEXT NOT NULL, is_distraction INTEGER NOT NULL DEFAULT 0 CHECK(is_distraction IN(0,1)), priority INTEGER NOT NULL DEFAULT 0, enabled INTEGER NOT NULL DEFAULT 1 CHECK(enabled IN(0,1)), created_at INTEGER NOT NULL, updated_at INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS excluded_applications(application_name TEXT PRIMARY KEY COLLATE NOCASE);
CREATE TABLE IF NOT EXISTS excluded_categories(category TEXT PRIMARY KEY COLLATE NOCASE);
CREATE TABLE IF NOT EXISTS activity_events(event_id INTEGER PRIMARY KEY, application_id INTEGER REFERENCES applications(application_id) ON DELETE SET NULL, timestamp INTEGER NOT NULL, event_type TEXT NOT NULL, window_title TEXT NOT NULL DEFAULT '');
CREATE TABLE IF NOT EXISTS idle_periods(idle_id INTEGER PRIMARY KEY, start_time INTEGER NOT NULL, end_time INTEGER NOT NULL, duration_ms INTEGER NOT NULL CHECK(duration_ms>=0), CHECK(end_time>=start_time));
CREATE TABLE IF NOT EXISTS sessions(session_id INTEGER PRIMARY KEY, application_id INTEGER REFERENCES applications(application_id) ON DELETE SET NULL, category TEXT NOT NULL DEFAULT 'Other', start_time INTEGER NOT NULL, end_time INTEGER NOT NULL, duration_ms INTEGER NOT NULL CHECK(duration_ms>=0), interruption_count INTEGER NOT NULL DEFAULT 0 CHECK(interruption_count>=0), context_switch_count INTEGER NOT NULL DEFAULT 0 CHECK(context_switch_count>=0), is_focus INTEGER NOT NULL DEFAULT 0 CHECK(is_focus IN(0,1)), is_deep_work INTEGER NOT NULL DEFAULT 0 CHECK(is_deep_work IN(0,1)), CHECK(end_time>=start_time));
CREATE TABLE IF NOT EXISTS context_switches(switch_id INTEGER PRIMARY KEY, previous_application_id INTEGER REFERENCES applications(application_id) ON DELETE SET NULL, new_application_id INTEGER REFERENCES applications(application_id) ON DELETE SET NULL, timestamp INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS goals(goal_id INTEGER PRIMARY KEY, name TEXT NOT NULL, metric TEXT NOT NULL, target_value REAL NOT NULL, period TEXT NOT NULL, active INTEGER NOT NULL DEFAULT 1 CHECK(active IN(0,1)), created_at INTEGER NOT NULL, completed_at INTEGER);
CREATE TABLE IF NOT EXISTS experiments(experiment_id INTEGER PRIMARY KEY, name TEXT NOT NULL, hypothesis TEXT NOT NULL DEFAULT '', intervention TEXT NOT NULL DEFAULT '', start_time INTEGER NOT NULL, end_time INTEGER, status TEXT NOT NULL DEFAULT 'planned', baseline_value REAL, result_value REAL, notes TEXT NOT NULL DEFAULT '', created_at INTEGER NOT NULL, CHECK(end_time IS NULL OR end_time>=start_time));
CREATE TABLE IF NOT EXISTS insights(insight_id INTEGER PRIMARY KEY, kind TEXT NOT NULL, title TEXT NOT NULL, message TEXT NOT NULL, score REAL NOT NULL DEFAULT 0, generated_at INTEGER NOT NULL, period_start INTEGER, period_end INTEGER, dismissed INTEGER NOT NULL DEFAULT 0 CHECK(dismissed IN(0,1)));
CREATE INDEX IF NOT EXISTS idx_activity_events_timestamp ON activity_events(timestamp);
CREATE INDEX IF NOT EXISTS idx_activity_events_app_time ON activity_events(application_id,timestamp);
CREATE INDEX IF NOT EXISTS idx_sessions_start ON sessions(start_time);
CREATE INDEX IF NOT EXISTS idx_sessions_app_start ON sessions(application_id,start_time);
CREATE INDEX IF NOT EXISTS idx_sessions_category_start ON sessions(category,start_time);
CREATE INDEX IF NOT EXISTS idx_idle_periods_start ON idle_periods(start_time);
CREATE INDEX IF NOT EXISTS idx_context_switches_time ON context_switches(timestamp);
CREATE INDEX IF NOT EXISTS idx_rules_priority ON classification_rules(enabled,priority DESC);
CREATE INDEX IF NOT EXISTS idx_insights_generated ON insights(generated_at);
INSERT OR IGNORE INTO schema_migrations(version,applied_at) VALUES(1,CAST(strftime('%s','now') AS INTEGER)*1000);
)sql";

Timestamp nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

[[noreturn]] void fail(sqlite3* db, std::string_view operation) {
    throw std::runtime_error(std::string(operation) + ": " + (db ? sqlite3_errmsg(db) : "SQLite error"));
}

class Statement {
public:
    Statement(sqlite3* db, std::string_view sql) : db_(db) {
        if (sqlite3_prepare_v2(db, sql.data(), static_cast<int>(sql.size()), &stmt_, nullptr) != SQLITE_OK) {
            fail(db, "prepare");
        }
    }
    ~Statement() { sqlite3_finalize(stmt_); }
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    void text(int index, std::string_view value) {
        if (sqlite3_bind_text(stmt_, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT) != SQLITE_OK) fail(db_, "bind text");
    }
    void integer(int index, std::int64_t value) {
        if (sqlite3_bind_int64(stmt_, index, value) != SQLITE_OK) fail(db_, "bind integer");
    }
    void real(int index, double value) {
        if (sqlite3_bind_double(stmt_, index, value) != SQLITE_OK) fail(db_, "bind real");
    }
    void nullableInteger(int index, const std::optional<std::int64_t>& value) {
        if (value) integer(index, *value);
        else if (sqlite3_bind_null(stmt_, index) != SQLITE_OK) fail(db_, "bind null");
    }
    void nullableReal(int index, const std::optional<double>& value) {
        if (value) real(index, *value);
        else if (sqlite3_bind_null(stmt_, index) != SQLITE_OK) fail(db_, "bind null");
    }
    bool row() {
        const int rc = sqlite3_step(stmt_);
        if (rc == SQLITE_ROW) return true;
        if (rc == SQLITE_DONE) return false;
        fail(db_, "step");
    }
    void run() {
        if (sqlite3_step(stmt_) != SQLITE_DONE) fail(db_, "execute statement");
    }
    std::int64_t integer(int column) const { return sqlite3_column_int64(stmt_, column); }
    double real(int column) const { return sqlite3_column_double(stmt_, column); }
    bool isNull(int column) const { return sqlite3_column_type(stmt_, column) == SQLITE_NULL; }
    std::string text(int column) const {
        const auto* value = sqlite3_column_text(stmt_, column);
        return value ? reinterpret_cast<const char*>(value) : "";
    }

private:
    sqlite3* db_{};
    sqlite3_stmt* stmt_{};
};

std::int64_t insertedId(sqlite3* db) { return sqlite3_last_insert_rowid(db); }

std::string csv(std::string_view value) {
    bool quote = value.find_first_of(",\"\r\n") != std::string_view::npos;
    if (!quote) return std::string(value);
    std::string result{"\""};
    for (char c : value) {
        result += c;
        if (c == '"') result += '"';
    }
    result += '"';
    return result;
}

std::string json(std::string_view value) {
    std::string result;
    result.reserve(value.size() + 2);
    result += '"';
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char c : value) {
        switch (c) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\b': result += "\\b"; break;
        case '\f': result += "\\f"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (c < 0x20) {
                result += "\\u00";
                result += hex[c >> 4];
                result += hex[c & 0x0f];
            } else {
                result += static_cast<char>(c);
            }
        }
    }
    result += '"';
    return result;
}

void validate(DateRange range) {
    if (range.end < range.start) throw std::invalid_argument("date range end precedes start");
}

} // namespace

Database::Database(const std::string& path) {
    const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    if (sqlite3_open_v2(path.c_str(), &db_, flags, nullptr) != SQLITE_OK) {
        const std::string message = db_ ? sqlite3_errmsg(db_) : "unable to allocate SQLite handle";
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("open database: " + message);
    }
    sqlite3_busy_timeout(db_, 5000);
    try {
        execute("PRAGMA foreign_keys=ON;");
        if (path != ":memory:") execute("PRAGMA journal_mode=WAL;");
    } catch (...) {
        sqlite3_close(db_);
        db_ = nullptr;
        throw;
    }
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

Database::Database(Database&& other) noexcept : db_(std::exchange(other.db_, nullptr)) {}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        if (db_) sqlite3_close(db_);
        db_ = std::exchange(other.db_, nullptr);
    }
    return *this;
}

Database Database::inMemory() { return Database(":memory:"); }
bool Database::isOpen() const noexcept { return db_ != nullptr; }
sqlite3* Database::nativeHandle() noexcept { return db_; }

void Database::execute(std::string_view sql) const {
    char* error = nullptr;
    const int rc = sqlite3_exec(db_, std::string(sql).c_str(), nullptr, nullptr, &error);
    if (rc != SQLITE_OK) {
        const std::string message = error ? error : sqlite3_errmsg(db_);
        sqlite3_free(error);
        throw std::runtime_error("execute SQL: " + message);
    }
}

void Database::migrate(std::string_view migration_sql) {
    Transaction transaction(*this);
    execute(migration_sql.empty() ? kSchema : migration_sql);
    transaction.commit();
}

void Database::beginTransaction() { execute("BEGIN IMMEDIATE;"); }
void Database::commit() { execute("COMMIT;"); }
void Database::rollback() noexcept {
    if (db_) sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
}

void Database::setSetting(std::string_view key, std::string_view value) {
    Statement s(db_, "INSERT INTO settings(key,value,updated_at) VALUES(?,?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value,updated_at=excluded.updated_at");
    s.text(1, key); s.text(2, value); s.integer(3, nowMs()); s.run();
}

std::optional<std::string> Database::setting(std::string_view key) const {
    Statement s(db_, "SELECT value FROM settings WHERE key=?");
    s.text(1, key);
    if (!s.row()) return std::nullopt;
    return s.text(0);
}

void Database::removeSetting(std::string_view key) {
    Statement s(db_, "DELETE FROM settings WHERE key=?"); s.text(1, key); s.run();
}

std::int64_t Database::upsertApplication(const Application& a) {
    Statement s(db_, "INSERT INTO applications(name,category,is_distraction,created_at) VALUES(?,?,?,?) ON CONFLICT(name) DO UPDATE SET category=excluded.category,is_distraction=excluded.is_distraction RETURNING application_id");
    s.text(1, a.name); s.text(2, a.category); s.integer(3, a.is_distraction); s.integer(4, a.created_at ? a.created_at : nowMs());
    if (!s.row()) fail(db_, "upsert application");
    return s.integer(0);
}

std::optional<Application> Database::application(std::int64_t id) const {
    Statement s(db_, "SELECT application_id,name,category,is_distraction,created_at FROM applications WHERE application_id=?");
    s.integer(1, id);
    if (!s.row()) return std::nullopt;
    return Application{s.integer(0), s.text(1), s.text(2), s.integer(3) != 0, s.integer(4)};
}

std::optional<Application> Database::applicationByName(std::string_view name) const {
    Statement s(db_, "SELECT application_id,name,category,is_distraction,created_at FROM applications WHERE name=? COLLATE NOCASE");
    s.text(1, name);
    if (!s.row()) return std::nullopt;
    return Application{s.integer(0), s.text(1), s.text(2), s.integer(3) != 0, s.integer(4)};
}

std::vector<Application> Database::applications() const {
    Statement s(db_, "SELECT application_id,name,category,is_distraction,created_at FROM applications ORDER BY name COLLATE NOCASE");
    std::vector<Application> result;
    while (s.row()) result.push_back({s.integer(0), s.text(1), s.text(2), s.integer(3) != 0, s.integer(4)});
    return result;
}

void Database::removeApplication(std::int64_t id) {
    Statement s(db_, "DELETE FROM applications WHERE application_id=?"); s.integer(1, id); s.run();
}

std::int64_t Database::saveClassificationRule(const ClassificationRule& r) {
    if (r.id == 0) {
        Statement s(db_, "INSERT INTO classification_rules(application_pattern,title_pattern,category,is_distraction,priority,enabled,created_at,updated_at) VALUES(?,?,?,?,?,?,?,?)");
        const auto timestamp = nowMs();
        s.text(1, r.application_pattern); s.text(2, r.title_pattern); s.text(3, r.category);
        s.integer(4, r.is_distraction); s.integer(5, r.priority); s.integer(6, r.enabled);
        s.integer(7, r.created_at ? r.created_at : timestamp); s.integer(8, timestamp); s.run();
        return insertedId(db_);
    }
    Statement s(db_, "UPDATE classification_rules SET application_pattern=?,title_pattern=?,category=?,is_distraction=?,priority=?,enabled=?,updated_at=? WHERE rule_id=?");
    s.text(1, r.application_pattern); s.text(2, r.title_pattern); s.text(3, r.category);
    s.integer(4, r.is_distraction); s.integer(5, r.priority); s.integer(6, r.enabled);
    s.integer(7, nowMs()); s.integer(8, r.id); s.run();
    return r.id;
}

std::vector<ClassificationRule> Database::classificationRules() const {
    Statement s(db_, "SELECT rule_id,application_pattern,title_pattern,category,is_distraction,priority,enabled,created_at,updated_at FROM classification_rules ORDER BY priority DESC,rule_id");
    std::vector<ClassificationRule> result;
    while (s.row()) result.push_back({s.integer(0),s.text(1),s.text(2),s.text(3),s.integer(4)!=0,static_cast<int>(s.integer(5)),s.integer(6)!=0,s.integer(7),s.integer(8)});
    return result;
}

void Database::removeClassificationRule(std::int64_t id) {
    Statement s(db_, "DELETE FROM classification_rules WHERE rule_id=?"); s.integer(1, id); s.run();
}

void Database::excludeApplication(std::string_view name) {
    Statement s(db_, "INSERT OR IGNORE INTO excluded_applications(application_name) VALUES(?)"); s.text(1, name); s.run();
}
void Database::includeApplication(std::string_view name) {
    Statement s(db_, "DELETE FROM excluded_applications WHERE application_name=? COLLATE NOCASE"); s.text(1, name); s.run();
}
bool Database::isApplicationExcluded(std::string_view name) const {
    Statement s(db_, "SELECT 1 FROM excluded_applications WHERE application_name=? COLLATE NOCASE"); s.text(1, name); return s.row();
}
std::vector<std::string> Database::excludedApplications() const {
    Statement s(db_, "SELECT application_name FROM excluded_applications ORDER BY application_name COLLATE NOCASE");
    std::vector<std::string> result; while (s.row()) result.push_back(s.text(0)); return result;
}
void Database::excludeCategory(std::string_view category) {
    Statement s(db_, "INSERT OR IGNORE INTO excluded_categories(category) VALUES(?)"); s.text(1, category); s.run();
}
void Database::includeCategory(std::string_view category) {
    Statement s(db_, "DELETE FROM excluded_categories WHERE category=? COLLATE NOCASE"); s.text(1, category); s.run();
}
bool Database::isCategoryExcluded(std::string_view category) const {
    Statement s(db_, "SELECT 1 FROM excluded_categories WHERE category=? COLLATE NOCASE"); s.text(1, category); return s.row();
}
std::vector<std::string> Database::excludedCategories() const {
    Statement s(db_, "SELECT category FROM excluded_categories ORDER BY category COLLATE NOCASE");
    std::vector<std::string> result; while (s.row()) result.push_back(s.text(0)); return result;
}

std::int64_t Database::addActivityEvent(const ActivityEvent& e) {
    Statement s(db_, "INSERT INTO activity_events(application_id,timestamp,event_type,window_title) VALUES(?,?,?,?)");
    s.nullableInteger(1,e.application_id); s.integer(2,e.timestamp); s.text(3,e.event_type); s.text(4,e.window_title); s.run(); return insertedId(db_);
}
std::int64_t Database::addIdlePeriod(const IdlePeriod& p) {
    Statement s(db_, "INSERT INTO idle_periods(start_time,end_time,duration_ms) VALUES(?,?,?)");
    s.integer(1,p.start_time); s.integer(2,p.end_time); s.integer(3,p.duration_ms); s.run(); return insertedId(db_);
}
std::int64_t Database::addSession(const Session& v) {
    Statement s(db_, "INSERT INTO sessions(application_id,category,start_time,end_time,duration_ms,interruption_count,context_switch_count,is_focus,is_deep_work) VALUES(?,?,?,?,?,?,?,?,?)");
    s.nullableInteger(1,v.application_id); s.text(2,v.category); s.integer(3,v.start_time); s.integer(4,v.end_time); s.integer(5,v.duration_ms);
    s.integer(6,v.interruption_count); s.integer(7,v.context_switch_count); s.integer(8,v.is_focus); s.integer(9,v.is_deep_work); s.run(); return insertedId(db_);
}
std::int64_t Database::addContextSwitch(const ContextSwitch& c) {
    Statement s(db_, "INSERT INTO context_switches(previous_application_id,new_application_id,timestamp) VALUES(?,?,?)");
    s.nullableInteger(1,c.previous_application_id); s.nullableInteger(2,c.new_application_id); s.integer(3,c.timestamp); s.run(); return insertedId(db_);
}

std::vector<ActivityEvent> Database::activityEvents(DateRange range) const {
    validate(range); Statement s(db_, "SELECT event_id,application_id,timestamp,event_type,window_title FROM activity_events WHERE timestamp>=? AND timestamp<? ORDER BY timestamp,event_id");
    s.integer(1,range.start); s.integer(2,range.end); std::vector<ActivityEvent> result;
    while(s.row()) result.push_back({s.integer(0),s.isNull(1)?std::nullopt:std::optional<std::int64_t>{s.integer(1)},s.integer(2),s.text(3),s.text(4)}); return result;
}
std::vector<IdlePeriod> Database::idlePeriods(DateRange range) const {
    validate(range); Statement s(db_, "SELECT idle_id,start_time,end_time,duration_ms FROM idle_periods WHERE start_time<? AND end_time>? ORDER BY start_time,idle_id");
    s.integer(1,range.end); s.integer(2,range.start); std::vector<IdlePeriod> result;
    while(s.row()) result.push_back({s.integer(0),s.integer(1),s.integer(2),s.integer(3)}); return result;
}
std::vector<Session> Database::sessions(DateRange range) const {
    validate(range); Statement s(db_, "SELECT session_id,application_id,category,start_time,end_time,duration_ms,interruption_count,context_switch_count,is_focus,is_deep_work FROM sessions WHERE start_time<? AND end_time>? ORDER BY start_time,session_id");
    s.integer(1,range.end); s.integer(2,range.start); std::vector<Session> result;
    while(s.row()) result.push_back({s.integer(0),s.isNull(1)?std::nullopt:std::optional<std::int64_t>{s.integer(1)},s.text(2),s.integer(3),s.integer(4),s.integer(5),static_cast<int>(s.integer(6)),static_cast<int>(s.integer(7)),s.integer(8)!=0,s.integer(9)!=0}); return result;
}
std::vector<ContextSwitch> Database::contextSwitches(DateRange range) const {
    validate(range); Statement s(db_, "SELECT switch_id,previous_application_id,new_application_id,timestamp FROM context_switches WHERE timestamp>=? AND timestamp<? ORDER BY timestamp,switch_id");
    s.integer(1,range.start); s.integer(2,range.end); std::vector<ContextSwitch> result;
    while(s.row()) result.push_back({s.integer(0),s.isNull(1)?std::nullopt:std::optional<std::int64_t>{s.integer(1)},s.isNull(2)?std::nullopt:std::optional<std::int64_t>{s.integer(2)},s.integer(3)}); return result;
}

void Database::deleteRange(DateRange range) {
    validate(range); Transaction t(*this);
    for (const auto* sql : {
        "DELETE FROM activity_events WHERE timestamp>=? AND timestamp<?",
        "DELETE FROM context_switches WHERE timestamp>=? AND timestamp<?",
        "DELETE FROM idle_periods WHERE start_time<? AND end_time>?",
        "DELETE FROM sessions WHERE start_time<? AND end_time>?"}) {
        Statement s(db_,sql);
        if (std::string_view(sql).find("timestamp") != std::string_view::npos) { s.integer(1,range.start); s.integer(2,range.end); }
        else { s.integer(1,range.end); s.integer(2,range.start); }
        s.run();
    }
    t.commit();
}

void Database::deleteAllActivity() {
    Transaction t(*this);
    execute("DELETE FROM activity_events;DELETE FROM context_switches;DELETE FROM idle_periods;DELETE FROM sessions;");
    t.commit();
}

std::int64_t Database::cleanupRetention(Timestamp cutoff) {
    Transaction t(*this); std::int64_t count=0;
    for (const auto* sql : {"DELETE FROM activity_events WHERE timestamp<?","DELETE FROM context_switches WHERE timestamp<?","DELETE FROM idle_periods WHERE end_time<?","DELETE FROM sessions WHERE end_time<?"}) {
        Statement s(db_,sql); s.integer(1,cutoff); s.run(); count += sqlite3_changes(db_);
    }
    t.commit(); return count;
}

std::int64_t Database::saveGoal(const Goal& g) {
    if(g.id==0) {
        Statement s(db_,"INSERT INTO goals(name,metric,target_value,period,active,created_at,completed_at) VALUES(?,?,?,?,?,?,?)");
        s.text(1,g.name);s.text(2,g.metric);s.real(3,g.target_value);s.text(4,g.period);s.integer(5,g.active);s.integer(6,g.created_at?g.created_at:nowMs());s.nullableInteger(7,g.completed_at);s.run();return insertedId(db_);
    }
    Statement s(db_,"UPDATE goals SET name=?,metric=?,target_value=?,period=?,active=?,completed_at=? WHERE goal_id=?");
    s.text(1,g.name);s.text(2,g.metric);s.real(3,g.target_value);s.text(4,g.period);s.integer(5,g.active);s.nullableInteger(6,g.completed_at);s.integer(7,g.id);s.run();return g.id;
}
std::vector<Goal> Database::goals(bool active_only) const {
    Statement s(db_,active_only?"SELECT goal_id,name,metric,target_value,period,active,created_at,completed_at FROM goals WHERE active=1 ORDER BY created_at DESC":"SELECT goal_id,name,metric,target_value,period,active,created_at,completed_at FROM goals ORDER BY created_at DESC");
    std::vector<Goal> r;while(s.row())r.push_back({s.integer(0),s.text(1),s.text(2),s.real(3),s.text(4),s.integer(5)!=0,s.integer(6),s.isNull(7)?std::nullopt:std::optional<Timestamp>{s.integer(7)}});return r;
}
void Database::removeGoal(std::int64_t id){Statement s(db_,"DELETE FROM goals WHERE goal_id=?");s.integer(1,id);s.run();}

std::int64_t Database::saveExperiment(const Experiment& e) {
    if(e.id==0) {
        Statement s(db_,"INSERT INTO experiments(name,hypothesis,intervention,start_time,end_time,status,baseline_value,result_value,notes,created_at) VALUES(?,?,?,?,?,?,?,?,?,?)");
        s.text(1,e.name);s.text(2,e.hypothesis);s.text(3,e.intervention);s.integer(4,e.start_time);s.nullableInteger(5,e.end_time);s.text(6,e.status);s.nullableReal(7,e.baseline_value);s.nullableReal(8,e.result_value);s.text(9,e.notes);s.integer(10,e.created_at?e.created_at:nowMs());s.run();return insertedId(db_);
    }
    Statement s(db_,"UPDATE experiments SET name=?,hypothesis=?,intervention=?,start_time=?,end_time=?,status=?,baseline_value=?,result_value=?,notes=? WHERE experiment_id=?");
    s.text(1,e.name);s.text(2,e.hypothesis);s.text(3,e.intervention);s.integer(4,e.start_time);s.nullableInteger(5,e.end_time);s.text(6,e.status);s.nullableReal(7,e.baseline_value);s.nullableReal(8,e.result_value);s.text(9,e.notes);s.integer(10,e.id);s.run();return e.id;
}
std::vector<Experiment> Database::experiments() const {
    Statement s(db_,"SELECT experiment_id,name,hypothesis,intervention,start_time,end_time,status,baseline_value,result_value,notes,created_at FROM experiments ORDER BY start_time DESC");
    std::vector<Experiment> r;while(s.row())r.push_back({s.integer(0),s.text(1),s.text(2),s.text(3),s.integer(4),s.isNull(5)?std::nullopt:std::optional<Timestamp>{s.integer(5)},s.text(6),s.isNull(7)?std::nullopt:std::optional<double>{s.real(7)},s.isNull(8)?std::nullopt:std::optional<double>{s.real(8)},s.text(9),s.integer(10)});return r;
}
void Database::removeExperiment(std::int64_t id){Statement s(db_,"DELETE FROM experiments WHERE experiment_id=?");s.integer(1,id);s.run();}

std::int64_t Database::saveInsight(const Insight& i) {
    if(i.id==0) {
        Statement s(db_,"INSERT INTO insights(kind,title,message,score,generated_at,period_start,period_end,dismissed) VALUES(?,?,?,?,?,?,?,?)");
        s.text(1,i.kind);s.text(2,i.title);s.text(3,i.message);s.real(4,i.score);s.integer(5,i.generated_at?i.generated_at:nowMs());s.nullableInteger(6,i.period_start);s.nullableInteger(7,i.period_end);s.integer(8,i.dismissed);s.run();return insertedId(db_);
    }
    Statement s(db_,"UPDATE insights SET kind=?,title=?,message=?,score=?,generated_at=?,period_start=?,period_end=?,dismissed=? WHERE insight_id=?");
    s.text(1,i.kind);s.text(2,i.title);s.text(3,i.message);s.real(4,i.score);s.integer(5,i.generated_at);s.nullableInteger(6,i.period_start);s.nullableInteger(7,i.period_end);s.integer(8,i.dismissed);s.integer(9,i.id);s.run();return i.id;
}
std::vector<Insight> Database::insights(DateRange range) const {
    validate(range);Statement s(db_,"SELECT insight_id,kind,title,message,score,generated_at,period_start,period_end,dismissed FROM insights WHERE generated_at>=? AND generated_at<? ORDER BY generated_at DESC");
    s.integer(1,range.start);s.integer(2,range.end);std::vector<Insight> r;while(s.row())r.push_back({s.integer(0),s.text(1),s.text(2),s.text(3),s.real(4),s.integer(5),s.isNull(6)?std::nullopt:std::optional<Timestamp>{s.integer(6)},s.isNull(7)?std::nullopt:std::optional<Timestamp>{s.integer(7)},s.integer(8)!=0});return r;
}
void Database::dismissInsight(std::int64_t id,bool dismissed){Statement s(db_,"UPDATE insights SET dismissed=? WHERE insight_id=?");s.integer(1,dismissed);s.integer(2,id);s.run();}
void Database::removeInsight(std::int64_t id){Statement s(db_,"DELETE FROM insights WHERE insight_id=?");s.integer(1,id);s.run();}

std::string Database::exportCsv(DateRange range) const {
    validate(range);
    Statement s(db_,"SELECT e.timestamp,COALESCE(a.name,''),COALESCE(a.category,''),e.event_type,e.window_title FROM activity_events e LEFT JOIN applications a ON a.application_id=e.application_id WHERE e.timestamp>=? AND e.timestamp<? ORDER BY e.timestamp,e.event_id");
    s.integer(1,range.start);s.integer(2,range.end);
    std::ostringstream out;out<<"timestamp,application,category,event_type,window_title\n";
    while(s.row())out<<s.integer(0)<<','<<csv(s.text(1))<<','<<csv(s.text(2))<<','<<csv(s.text(3))<<','<<csv(s.text(4))<<'\n';
    return out.str();
}

std::string Database::exportJson(DateRange range) const {
    validate(range);
    Statement s(db_,"SELECT e.event_id,e.timestamp,e.application_id,COALESCE(a.name,''),COALESCE(a.category,''),e.event_type,e.window_title FROM activity_events e LEFT JOIN applications a ON a.application_id=e.application_id WHERE e.timestamp>=? AND e.timestamp<? ORDER BY e.timestamp,e.event_id");
    s.integer(1,range.start);s.integer(2,range.end);
    std::ostringstream out;out<<"{\"activity_events\":[";bool first=true;
    while(s.row()){if(!first)out<<',';first=false;out<<"{\"id\":"<<s.integer(0)<<",\"timestamp\":"<<s.integer(1)<<",\"application_id\":";if(s.isNull(2))out<<"null";else out<<s.integer(2);out<<",\"application\":"<<json(s.text(3))<<",\"category\":"<<json(s.text(4))<<",\"event_type\":"<<json(s.text(5))<<",\"window_title\":"<<json(s.text(6))<<'}';}
    out<<"]}";return out.str();
}

Transaction::Transaction(Database& database):database_(&database){database_->beginTransaction();}
Transaction::~Transaction(){if(!finished_&&database_)database_->rollback();}
void Transaction::commit(){if(finished_)throw std::logic_error("transaction already finished");database_->commit();finished_=true;}
void Transaction::rollback() noexcept{if(!finished_&&database_){database_->rollback();finished_=true;}}

} // namespace activityos::storage
