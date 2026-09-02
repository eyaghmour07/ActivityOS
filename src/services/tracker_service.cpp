#include "activityos/services.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <stdexcept>
#include <unordered_map>

namespace activityos {
namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool containsInsensitive(const std::string& value, const std::string& pattern) {
    return pattern.empty() || lower(value).find(lower(pattern)) != std::string::npos;
}

std::pair<std::string, bool> defaultClassification(const std::string& application) {
    const auto app = lower(application);
    const auto containsAny = [&](std::initializer_list<const char*> names) {
        return std::any_of(names.begin(), names.end(), [&](const char* name) {
            return app.find(name) != std::string::npos;
        });
    };
    if (containsAny({"visual studio code", "vscode", "xcode", "terminal", "iterm",
                     "visual studio", "intellij", "clion", "pycharm", "android studio"})) {
        return {"Coding", false};
    }
    if (containsAny({"slack", "discord", "teams", "zoom", "mail", "messages"})) {
        return {"Communication", false};
    }
    if (containsAny({"youtube", "netflix", "spotify", "steam", "twitch"})) {
        return {"Entertainment", true};
    }
    if (containsAny({"notion", "obsidian", "todoist", "calendar"})) {
        return {"Planning", false};
    }
    if (containsAny({"chrome", "safari", "firefox", "edge", "arc"})) {
        return {"General", false};
    }
    return {"Other", false};
}

} // namespace

std::int64_t unixMillisecondsNow() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

TrackerService::TrackerService(storage::Database& database,
                               std::unique_ptr<ActivitySource> source,
                               TrackerConfig config)
    : database_(database), source_(std::move(source)), config_(config) {
    if (!source_) {
        throw std::invalid_argument("TrackerService requires an activity source");
    }
}

TrackerService::~TrackerService() {
    try {
        shutdown(unixMillisecondsNow());
    } catch (...) {
    }
}

storage::Application TrackerService::classify(const ActivitySnapshot& snapshot,
                                              std::int64_t now_unix_ms) {
    auto [category, distraction] = defaultClassification(snapshot.application_name);
    auto rules = database_.classificationRules();
    std::stable_sort(rules.begin(), rules.end(), [](const auto& left, const auto& right) {
        return left.priority > right.priority;
    });
    const std::string title = snapshot.window_title.value_or("");
    for (const auto& rule : rules) {
        if (rule.enabled &&
            containsInsensitive(snapshot.application_name, rule.application_pattern) &&
            containsInsensitive(title, rule.title_pattern)) {
            category = rule.category;
            distraction = rule.is_distraction;
            break;
        }
    }

    storage::Application application;
    if (const auto existing = database_.applicationByName(snapshot.application_name)) {
        application = *existing;
        if (application.category == category &&
            application.is_distraction == distraction) {
            return application;
        }
    }
    application.name = snapshot.application_name;
    application.category = category;
    application.is_distraction = distraction;
    application.created_at = application.created_at == 0 ? now_unix_ms : application.created_at;
    application.id = database_.upsertApplication(application);
    return application;
}

void TrackerService::recordEvent(std::optional<std::int64_t> application_id,
                                 std::int64_t timestamp,
                                 std::string type,
                                 const std::optional<std::string>& title) {
    storage::ActivityEvent event;
    event.application_id = application_id;
    event.timestamp = timestamp;
    event.event_type = std::move(type);
    if (config_.persist_window_titles && title) {
        event.window_title = *title;
    }
    database_.addActivityEvent(event);
    last_event_ms_ = timestamp;
}

void TrackerService::closeCurrent(std::int64_t end_unix_ms) {
    if (!current_) return;
    const auto boundedEnd = std::max(current_->start_ms, end_unix_ms);
    storage::Session session;
    session.application_id = current_->application_id;
    session.category = current_->category;
    session.start_time = current_->start_ms;
    session.end_time = boundedEnd;
    session.duration_ms = boundedEnd - current_->start_ms;
    if (session.duration_ms > 0) {
        database_.addSession(session);
    }
    current_.reset();
}

TrackerStatus TrackerService::poll(std::int64_t now_unix_ms) {
    if (status_.paused) return status_;

    const auto snapshot = source_->capture();
    status_.source_status = snapshot.metadata.status;
    status_.message = snapshot.metadata.error_message;
    if (snapshot.metadata.status == ActivitySourceStatus::unsupported ||
        snapshot.metadata.status == ActivitySourceStatus::error ||
        snapshot.application_name.empty()) {
        closeCurrent(now_unix_ms);
        status_.active_application.clear();
        status_.active_category.clear();
        return status_;
    }

    if (snapshot.idle_duration.count() >= config_.idle_threshold_ms) {
        if (!idle_started_ms_) {
            idle_started_ms_ = std::max<std::int64_t>(
                0, now_unix_ms - snapshot.idle_duration.count());
            closeCurrent(*idle_started_ms_);
            recordEvent(std::nullopt, *idle_started_ms_, "IDLE_START");
        }
        status_.idle = true;
        status_.active_application.clear();
        status_.active_category.clear();
        return status_;
    }

    if (idle_started_ms_) {
        storage::IdlePeriod idle;
        idle.start_time = *idle_started_ms_;
        idle.end_time = now_unix_ms;
        idle.duration_ms = std::max<std::int64_t>(0, idle.end_time - idle.start_time);
        database_.addIdlePeriod(idle);
        recordEvent(std::nullopt, now_unix_ms, "IDLE_END");
        idle_started_ms_.reset();
    }
    status_.idle = false;

    auto application = classify(snapshot, now_unix_ms);
    if (database_.isApplicationExcluded(application.name) ||
        database_.isCategoryExcluded(application.category)) {
        closeCurrent(now_unix_ms);
        status_.active_application = "Excluded";
        status_.active_category.clear();
        return status_;
    }

    const bool switched = current_ && current_->application_id != application.id;
    if (switched) {
        const auto previousId = current_->application_id;
        closeCurrent(now_unix_ms);
        storage::ContextSwitch contextSwitch;
        contextSwitch.previous_application_id = previousId;
        contextSwitch.new_application_id = application.id;
        contextSwitch.timestamp = now_unix_ms;
        database_.addContextSwitch(contextSwitch);
        recordEvent(application.id, now_unix_ms, "SWITCH", snapshot.window_title);
    }

    if (!current_) {
        current_ = CurrentSession{application.id, application.name, application.category,
                                  now_unix_ms, now_unix_ms};
        if (!switched) {
            recordEvent(application.id, now_unix_ms, "ACTIVE", snapshot.window_title);
        }
    } else {
        current_->last_seen_ms = now_unix_ms;
    }

    if (last_event_ms_ == 0 ||
        now_unix_ms - last_event_ms_ >= config_.heartbeat_interval_ms) {
        recordEvent(application.id, now_unix_ms, "HEARTBEAT", snapshot.window_title);
    }
    status_.active_application = application.name;
    status_.active_category = application.category;
    return status_;
}

void TrackerService::pause(std::int64_t now_unix_ms) {
    closeCurrent(now_unix_ms);
    if (idle_started_ms_) {
        storage::IdlePeriod idle{0, *idle_started_ms_, now_unix_ms,
                                 std::max<std::int64_t>(0, now_unix_ms - *idle_started_ms_)};
        database_.addIdlePeriod(idle);
        idle_started_ms_.reset();
    }
    recordEvent(std::nullopt, now_unix_ms, "PAUSED");
    status_.paused = true;
    status_.idle = false;
    status_.active_application.clear();
    status_.active_category.clear();
}

void TrackerService::resume() {
    status_.paused = false;
}

void TrackerService::shutdown(std::int64_t now_unix_ms) {
    closeCurrent(now_unix_ms);
}

void TrackerService::updateConfig(TrackerConfig config) {
    config_ = config;
}

TrackerStatus TrackerService::status() const {
    return status_;
}

} // namespace activityos
