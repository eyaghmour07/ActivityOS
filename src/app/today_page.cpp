#include "today_page.hpp"

#include "ui_helpers.hpp"
#include "ui_widgets.hpp"

#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QStringList>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <unordered_map>
#include <utility>

namespace activityos::ui {
namespace {

QColor categoryBarColor(const QString& category, bool distraction) {
    if (distraction || category == "Entertainment") return QColor("#EF4444");
    if (category == "Coding") return QColor("#00DFA2");
    if (category == "Communication") return QColor("#F59E0B");
    if (category == "Research" || category == "Work") return QColor("#7B8CDE");
    if (category == "Planning" || category == "Administration") return QColor("#A78BFA");
    return QColor("#7B8CDE");
}

double hourOfDay(qint64 unixMs) {
    const auto time = QDateTime::fromMSecsSinceEpoch(unixMs).time();
    return time.hour() + time.minute() / 60.0 + time.second() / 3600.0;
}

QString formatClockHour(double hour);

QString sessionTimelineFamily(const Session& session) {
    if (session.distraction || session.category == "Entertainment") return "distraction";
    if (session.category == "Communication") return "comms";
    if (session.productive) return "work";
    return {};
}

QString timelineFamilyTitle(const QString& family) {
    if (family == "work") return "Active work";
    if (family == "comms") return "Communication";
    if (family == "distraction") return "Distraction";
    return "Activity";
}

void appendUniqueCategory(QStringList& categories, const QString& category) {
    if (category.isEmpty() || categories.contains(category)) return;
    if (categories.size() >= 3) return;
    categories.push_back(category);
}

std::vector<TimelineBlock> buildTimelineBlocks(const std::vector<Session>& sessions) {
    constexpr qint64 kMergeGapMs = 8 * kMinuteMs;
    constexpr qint64 kMinKeepMs = 3 * kMinuteMs;

    struct RawBlock {
        qint64 start_unix_ms{};
        qint64 end_unix_ms{};
        QString family;
        QStringList categories;
    };

    std::vector<RawBlock> raw;
    raw.reserve(sessions.size());
    for (const auto& session : sessions) {
        const auto family = sessionTimelineFamily(session);
        if (family.isEmpty() || session.end_unix_ms <= session.start_unix_ms) continue;
        raw.push_back({session.start_unix_ms, session.end_unix_ms, family,
                       {QString::fromStdString(session.category)}});
    }
    std::sort(raw.begin(), raw.end(), [](const RawBlock& left, const RawBlock& right) {
        return left.start_unix_ms < right.start_unix_ms;
    });

    std::vector<RawBlock> merged;
    for (const auto& item : raw) {
        if (!merged.empty() && merged.back().family == item.family &&
            item.start_unix_ms - merged.back().end_unix_ms <= kMergeGapMs) {
            merged.back().end_unix_ms = std::max(merged.back().end_unix_ms, item.end_unix_ms);
            for (const auto& category : item.categories) {
                appendUniqueCategory(merged.back().categories, category);
            }
            continue;
        }
        merged.push_back(item);
    }

    std::vector<RawBlock> smoothed;
    for (std::size_t i = 0; i < merged.size(); ++i) {
        auto item = merged[i];
        const auto duration = item.end_unix_ms - item.start_unix_ms;
        if (duration < kMinKeepMs) {
            if (!smoothed.empty()) {
                smoothed.back().end_unix_ms =
                    std::max(smoothed.back().end_unix_ms, item.end_unix_ms);
                continue;
            }
            if (i + 1 < merged.size()) {
                merged[i + 1].start_unix_ms =
                    std::min(merged[i + 1].start_unix_ms, item.start_unix_ms);
                continue;
            }
        }
        if (!smoothed.empty() && smoothed.back().family == item.family &&
            item.start_unix_ms - smoothed.back().end_unix_ms <= kMergeGapMs) {
            smoothed.back().end_unix_ms = std::max(smoothed.back().end_unix_ms, item.end_unix_ms);
            for (const auto& category : item.categories) {
                appendUniqueCategory(smoothed.back().categories, category);
            }
            continue;
        }
        smoothed.push_back(item);
    }

    std::vector<TimelineBlock> blocks;
    blocks.reserve(smoothed.size());
    for (const auto& item : smoothed) {
        TimelineBlock block;
        block.startHour = hourOfDay(item.start_unix_ms);
        block.endHour = hourOfDay(item.end_unix_ms);
        block.type = item.family;
        block.label = timelineFamilyTitle(item.family);
        const auto minutes =
            std::max<qint64>(1, (item.end_unix_ms - item.start_unix_ms) / kMinuteMs);
        const auto categories = item.categories.join(", ");
        block.tooltip = QString("%1 – %2\n%3 · %4m%5")
                            .arg(formatClockHour(block.startHour), formatClockHour(block.endHour),
                                 block.label)
                            .arg(minutes)
                            .arg(categories.isEmpty() ? QString() : QString(" · %1").arg(categories));
        blocks.push_back(std::move(block));
    }
    return blocks;
}

QWidget* legendRow() {
    struct LegendItem { QString type; QString label; QString color; };
    const std::array<LegendItem, 3> items{{
        {"work", "Active work", "#7B8CDE"},
        {"comms", "Communication", "#F59E0B"},
        {"distraction", "Distraction", "#EF4444"},
    }};
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    for (const auto& item : items) {
        auto* chip = new QHBoxLayout;
        chip->setSpacing(6);
        auto* swatch = new QLabel;
        swatch->setFixedSize(10, 10);
        swatch->setStyleSheet(
            QString("background:%1;border-radius:2px;").arg(item.color));
        chip->addWidget(swatch);
        chip->addWidget(monoCaption(item.label));
        layout->addLayout(chip);
    }
    layout->addStretch();
    return row;
}

QString formatClockHour(double hour) {
    hour = std::clamp(hour, 0.0, 24.0);
    auto totalMinutes = static_cast<int>(std::lround(hour * 60.0));
    if (totalMinutes >= 24 * 60) totalMinutes = 24 * 60;
    const int hours = totalMinutes / 60;
    const int minutes = totalMinutes % 60;
    return QString("%1:%2").arg(hours, 2, 10, QChar('0')).arg(minutes, 2, 10, QChar('0'));
}

std::pair<double, double> activityTimelineWindow(const std::vector<Session>& sessions,
                                                 double nowHour) {
    constexpr double kPadHours = 0.75;
    constexpr double kMinSpanHours = 4.0;

    double start = nowHour - 1.0;
    double end = nowHour + 3.0;
    if (!sessions.empty()) {
        start = hourOfDay(sessions.front().start_unix_ms);
        end = hourOfDay(sessions.front().end_unix_ms);
        for (const auto& session : sessions) {
            start = std::min(start, hourOfDay(session.start_unix_ms));
            end = std::max(end, hourOfDay(session.end_unix_ms));
        }
        start -= kPadHours;
        end += kPadHours;
    }

    if (end - start < kMinSpanHours) {
        const auto extra = (kMinSpanHours - (end - start)) / 2.0;
        start -= extra;
        end += extra;
    }
    if (start < 0.0) {
        end -= start;
        start = 0.0;
    }
    if (end > 24.0) {
        start -= end - 24.0;
        end = 24.0;
    }
    start = std::max(0.0, start);
    end = std::min(24.0, std::max(end, start + 1.0));
    return {start, end};
}

} // namespace

TodayPage::TodayPage(ActivityService& activity, QWidget* parent)
    : DashboardPage(parent), activity_(activity) {
    auto* layout = contentLayout();

    auto* recording = makeRecordingIndicator(trackingState_);
    addPageHeader(layout, QDate::currentDate().toString("dddd, d MMM yyyy"),
                  "Today's Activity", recording);

    auto* cards = new QGridLayout;
    cards->setHorizontalSpacing(12);
    cards->setVerticalSpacing(12);
    cards->addWidget(metricCard("Active Time", activeValue_, activeSub_), 0, 0);
    cards->addWidget(metricCard("Deep Focus", deepWorkValue_, deepWorkSub_, true), 0, 1);
    cards->addWidget(metricCard("Distractions", distractionValue_, distractionSub_), 0, 2);
    cards->addWidget(metricCard("Focus Score", scoreValue_, scoreSub_, true), 0, 3);
    equalizeColumns(cards);
    layout->addLayout(cards);

    auto* timelineBody = new QWidget;
    auto* timelineLayout = new QVBoxLayout(timelineBody);
    timelineLayout->setContentsMargins(0, 0, 0, 0);
    timelineLayout->setSpacing(10);
    timelineCaption_ = monoCaption("Day Timeline  ·  active hours");
    timelineLayout->addWidget(timelineCaption_);
    dayTimeline_ = new DayTimelineWidget;
    timelineLayout->addWidget(dayTimeline_);
    timelineLayout->addWidget(legendRow());
    layout->addWidget(wrapSection("", timelineBody));

    appBreakdown_ = new AppBreakdownWidget;
    layout->addWidget(wrapSection("Application Breakdown", appBreakdown_));

    auto* bottom = new QGridLayout;
    bottom->setHorizontalSpacing(12);
    bottom->addWidget(metricCard("Context Switches", switchesValue_, switchesSub_), 0, 0);
    bottom->addWidget(metricCard("Avg Session Length", avgSessionValue_, avgSessionSub_, true),
                       0, 1);
    bottom->addWidget(metricCard("Recovery Time", recoveryValue_, recoverySub_), 0, 2);
    equalizeColumns(bottom);
    layout->addLayout(bottom);

    layout->addStretch();
}

void TodayPage::refresh(const DashboardSnapshot& snapshot) {
    activeValue_->setText(duration(snapshot.metrics.active_ms));
    if (snapshot.baseline.active_ms > 0) {
        const auto delta = snapshot.metrics.active_ms - snapshot.baseline.active_ms;
        activeSub_->setText(delta >= 0
                                ? QString("↑ %1 vs typical day").arg(duration(delta))
                                : QString("↓ %1 vs typical day").arg(duration(-delta)));
    } else {
        activeSub_->setText("tracked today");
    }

    deepWorkValue_->setText(duration(snapshot.metrics.deep_work_ms));
    deepWorkSub_->setText(QString("%1 sessions").arg(snapshot.metrics.focus_session_count));

    distractionValue_->setText(duration(snapshot.metrics.distraction_ms));
    if (snapshot.baseline.distraction_ms > 0) {
        const auto delta = snapshot.metrics.distraction_ms - snapshot.baseline.distraction_ms;
        const auto pct = std::abs(delta) * 100.0 / snapshot.baseline.distraction_ms;
        distractionSub_->setText(delta <= 0 ? QString("↓ %1% vs baseline").arg(int(pct))
                                            : QString("↑ %1% vs baseline").arg(int(pct)));
    } else {
        distractionSub_->setText("building baseline");
    }

    scoreValue_->setText(QString("%1/100").arg(snapshot.score.score));
    scoreSub_->setText("productivity score");

    switchesValue_->setText(QString::number(snapshot.metrics.context_switch_count));
    switchesSub_->setText("disruptive switches");

    avgSessionValue_->setText(duration(snapshot.metrics.average_session_ms));
    if (snapshot.baseline.average_session_ms > 0) {
        const auto delta = snapshot.metrics.average_session_ms - snapshot.baseline.average_session_ms;
        avgSessionSub_->setText(delta >= 0 ? QString("↑ %1 vs baseline").arg(duration(delta))
                                          : QString("↓ %1 vs baseline").arg(duration(-delta)));
    } else {
        avgSessionSub_->setText("building baseline");
    }

    recoveryValue_->setText(snapshot.metrics.average_recovery_ms > 0
                                ? duration(snapshot.metrics.average_recovery_ms)
                                : "—");
    recoverySub_->setText("avg after distraction");

    const auto daySessions = activity_.sessions(todayRange());
    const auto nowHour = hourOfDay(QDateTime::currentMSecsSinceEpoch());
    const auto [timelineStart, timelineEnd] = activityTimelineWindow(daySessions, nowHour);
    dayTimeline_->setRange(timelineStart, timelineEnd);
    dayTimeline_->setBlocks(buildTimelineBlocks(daySessions));
    dayTimeline_->setNowHour(nowHour);
    const auto spanHours = timelineEnd - timelineStart;
    timelineCaption_->setText(
        QString("Day Timeline  ·  %1 – %2%3")
            .arg(formatClockHour(timelineStart), formatClockHour(timelineEnd),
                 spanHours > 12.0 ? "  ·  scroll to see the full day" : ""));

    std::unordered_map<std::string, std::int64_t> appMs;
    for (const auto& session : daySessions) {
        appMs[session.app] += session.active_duration_ms;
    }
    std::vector<std::pair<std::string, std::int64_t>> ranked(appMs.begin(), appMs.end());
    std::sort(ranked.begin(), ranked.end(),
              [](const auto& left, const auto& right) { return left.second > right.second; });
    const auto topTotal = ranked.empty()
                              ? 0
                              : std::accumulate(ranked.begin(), ranked.end(), std::int64_t{0},
                                                [](std::int64_t sum, const auto& item) {
                                                    return sum + item.second;
                                                });
    std::vector<AppUsageRow> rows;
    for (const auto& [name, ms] : ranked) {
        if (rows.size() >= 8) break;
        const auto category = std::find_if(daySessions.begin(), daySessions.end(),
                                           [&](const Session& session) {
                                               return session.app == name;
                                           });
        const QString categoryName =
            category != daySessions.end() ? QString::fromStdString(category->category) : "Other";
        AppUsageRow row;
        row.name = QString::fromStdString(name);
        row.minutes = static_cast<int>((ms + kMinuteMs / 2) / kMinuteMs);
        row.pct = topTotal > 0 ? static_cast<int>((ms * 100) / topTotal) : 0;
        row.color = categoryBarColor(categoryName, category != daySessions.end() && category->distraction);
        rows.push_back(row);
    }
    appBreakdown_->setRows(std::move(rows));
}

void TodayPage::redrawCharts() {
    dayTimeline_->update();
}

void TodayPage::setTrackingState(const QString& text, bool recording) {
    trackingState_->setObjectName(recording ? "recordingLabel" : "trackingPill");
    trackingState_->setText(text);
    trackingState_->style()->unpolish(trackingState_);
    trackingState_->style()->polish(trackingState_);
}

} // namespace activityos::ui
