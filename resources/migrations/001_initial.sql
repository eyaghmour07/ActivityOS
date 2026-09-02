PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS schema_migrations (
    version INTEGER PRIMARY KEY,
    applied_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS settings (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    updated_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS applications (
    application_id INTEGER PRIMARY KEY,
    name TEXT NOT NULL COLLATE NOCASE UNIQUE,
    category TEXT NOT NULL DEFAULT 'Other',
    is_distraction INTEGER NOT NULL DEFAULT 0 CHECK (is_distraction IN (0, 1)),
    created_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS classification_rules (
    rule_id INTEGER PRIMARY KEY,
    application_pattern TEXT NOT NULL,
    title_pattern TEXT NOT NULL DEFAULT '',
    category TEXT NOT NULL,
    is_distraction INTEGER NOT NULL DEFAULT 0 CHECK (is_distraction IN (0, 1)),
    priority INTEGER NOT NULL DEFAULT 0,
    enabled INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1)),
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS excluded_applications (
    application_name TEXT PRIMARY KEY COLLATE NOCASE
);

CREATE TABLE IF NOT EXISTS excluded_categories (
    category TEXT PRIMARY KEY COLLATE NOCASE
);

CREATE TABLE IF NOT EXISTS activity_events (
    event_id INTEGER PRIMARY KEY,
    application_id INTEGER REFERENCES applications(application_id) ON DELETE SET NULL,
    timestamp INTEGER NOT NULL,
    event_type TEXT NOT NULL,
    window_title TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS idle_periods (
    idle_id INTEGER PRIMARY KEY,
    start_time INTEGER NOT NULL,
    end_time INTEGER NOT NULL,
    duration_ms INTEGER NOT NULL CHECK (duration_ms >= 0),
    CHECK (end_time >= start_time)
);

CREATE TABLE IF NOT EXISTS sessions (
    session_id INTEGER PRIMARY KEY,
    application_id INTEGER REFERENCES applications(application_id) ON DELETE SET NULL,
    category TEXT NOT NULL DEFAULT 'Other',
    start_time INTEGER NOT NULL,
    end_time INTEGER NOT NULL,
    duration_ms INTEGER NOT NULL CHECK (duration_ms >= 0),
    interruption_count INTEGER NOT NULL DEFAULT 0 CHECK (interruption_count >= 0),
    context_switch_count INTEGER NOT NULL DEFAULT 0 CHECK (context_switch_count >= 0),
    is_focus INTEGER NOT NULL DEFAULT 0 CHECK (is_focus IN (0, 1)),
    is_deep_work INTEGER NOT NULL DEFAULT 0 CHECK (is_deep_work IN (0, 1)),
    CHECK (end_time >= start_time)
);

CREATE TABLE IF NOT EXISTS context_switches (
    switch_id INTEGER PRIMARY KEY,
    previous_application_id INTEGER REFERENCES applications(application_id) ON DELETE SET NULL,
    new_application_id INTEGER REFERENCES applications(application_id) ON DELETE SET NULL,
    timestamp INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS goals (
    goal_id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    metric TEXT NOT NULL,
    target_value REAL NOT NULL,
    period TEXT NOT NULL,
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    created_at INTEGER NOT NULL,
    completed_at INTEGER
);

CREATE TABLE IF NOT EXISTS experiments (
    experiment_id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    hypothesis TEXT NOT NULL DEFAULT '',
    intervention TEXT NOT NULL DEFAULT '',
    start_time INTEGER NOT NULL,
    end_time INTEGER,
    status TEXT NOT NULL DEFAULT 'planned',
    baseline_value REAL,
    result_value REAL,
    notes TEXT NOT NULL DEFAULT '',
    created_at INTEGER NOT NULL,
    CHECK (end_time IS NULL OR end_time >= start_time)
);

CREATE TABLE IF NOT EXISTS insights (
    insight_id INTEGER PRIMARY KEY,
    kind TEXT NOT NULL,
    title TEXT NOT NULL,
    message TEXT NOT NULL,
    score REAL NOT NULL DEFAULT 0,
    generated_at INTEGER NOT NULL,
    period_start INTEGER,
    period_end INTEGER,
    dismissed INTEGER NOT NULL DEFAULT 0 CHECK (dismissed IN (0, 1))
);

CREATE INDEX IF NOT EXISTS idx_activity_events_timestamp ON activity_events(timestamp);
CREATE INDEX IF NOT EXISTS idx_activity_events_app_time ON activity_events(application_id, timestamp);
CREATE INDEX IF NOT EXISTS idx_sessions_start ON sessions(start_time);
CREATE INDEX IF NOT EXISTS idx_sessions_app_start ON sessions(application_id, start_time);
CREATE INDEX IF NOT EXISTS idx_sessions_category_start ON sessions(category, start_time);
CREATE INDEX IF NOT EXISTS idx_idle_periods_start ON idle_periods(start_time);
CREATE INDEX IF NOT EXISTS idx_context_switches_time ON context_switches(timestamp);
CREATE INDEX IF NOT EXISTS idx_rules_priority ON classification_rules(enabled, priority DESC);
CREATE INDEX IF NOT EXISTS idx_insights_generated ON insights(generated_at);

INSERT OR IGNORE INTO schema_migrations(version, applied_at)
VALUES (1, CAST(strftime('%s', 'now') AS INTEGER) * 1000);
