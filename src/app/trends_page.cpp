#include "trends_page.hpp"

#include "ui_helpers.hpp"
#include "ui_widgets.hpp"

#include <QDateTime>
#include <QGridLayout>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

#include <array>
#include <utility>

namespace activityos::ui {

TrendsPage::TrendsPage(ActivityService& activity, QWidget* parent)
    : DashboardPage(parent), activity_(activity) {
    auto* layout = contentLayout();
    addPageHeader(layout, "Analytics", "Weekly Trends");

    auto* cards = new QGridLayout;
    cards->setHorizontalSpacing(12);
    cards->addWidget(metricCard("Avg Focus/Day", trendsAvgFocusValue_, trendsAvgFocusSub_, true),
                    0, 0);
    cards->addWidget(metricCard("Best Day", trendsBestDayValue_, trendsBestDaySub_), 0, 1);
    cards->addWidget(metricCard("Focus Score", trendsScoreValue_, trendsScoreSub_), 0, 2);
    cards->addWidget(metricCard("Streak", trendsStreakValue_, trendsStreakSub_, true), 0, 3);
    equalizeColumns(cards);
    layout->addLayout(cards);

    weeklyChart_ = new WeeklyChartWidget;
    layout->addWidget(wrapSection("Daily Breakdown  ·  Last 7 Days", weeklyChart_));

    auto* profileGrid = new QGridLayout;
    profileGrid->setHorizontalSpacing(32);
    profileGrid->setVerticalSpacing(16);
    workstylePeak_ = new QLabel("—");
    workstyleType_ = new QLabel("—");
    workstyleDayLength_ = new QLabel("—");
    workstyleDeepRatio_ = new QLabel("—");
    const auto addProfileCell = [&](int row, int col, const QString& label, QLabel*& value) {
        auto* cell = new QVBoxLayout;
        cell->setSpacing(4);
        cell->addWidget(monoCaption(label));
        value->setObjectName("profileValue");
        cell->addWidget(value);
        profileGrid->addLayout(cell, row, col);
    };
    addProfileCell(0, 0, "Peak hours", workstylePeak_);
    addProfileCell(0, 1, "Work type", workstyleType_);
    addProfileCell(1, 0, "Avg day length", workstyleDayLength_);
    addProfileCell(1, 1, "Deep work ratio", workstyleDeepRatio_);
    auto* profileWidget = new QWidget;
    profileWidget->setLayout(profileGrid);
    layout->addWidget(wrapSection("Workstyle Profile  ·  14-day Baseline", profileWidget));

    historyTable_ = table({"Date", "Active", "Focus", "Deep Work", "Distraction", "Switches"});
    layout->addWidget(wrapSection("Daily History", historyTable_), 1);
}

void TrendsPage::refresh(const DashboardSnapshot& snapshot) {
    const storage::DateRange weekRange{localDayStart(6), localDayEnd()};
    const auto days = activity_.dailyHistory(weekRange);
    double totalFocus = 0;
    int bestIndex = -1;
    std::int64_t bestFocus = 0;
    for (int index = 0; index < static_cast<int>(days.size()); ++index) {
        totalFocus += days[static_cast<std::size_t>(index)].focused_ms;
        if (days[static_cast<std::size_t>(index)].focused_ms >= bestFocus) {
            bestFocus = days[static_cast<std::size_t>(index)].focused_ms;
            bestIndex = index;
        }
    }
    trendsAvgFocusValue_->setText(days.empty()
                                      ? "—"
                                      : duration(static_cast<std::int64_t>(totalFocus / days.size())));
    trendsAvgFocusSub_->setText("7-day average");
    trendsBestDayValue_->setText(bestIndex >= 0
                                     ? QDateTime::fromMSecsSinceEpoch(
                                           days[static_cast<std::size_t>(bestIndex)].day_start_unix_ms)
                                           .date()
                                           .toString("ddd")
                                     : "—");
    trendsBestDaySub_->setText(bestIndex >= 0 ? duration(bestFocus) : "—");
    trendsScoreValue_->setText(QString("%1/100").arg(snapshot.score.score));
    trendsScoreSub_->setText("today's score");
    trendsStreakValue_->setText("—");
    trendsStreakSub_->setText("focus goal tracking");

    std::vector<WeekDayBars> chartDays;
    chartDays.reserve(days.size());
    for (const auto& day : days) {
        WeekDayBars bars;
        const auto date = QDateTime::fromMSecsSinceEpoch(day.day_start_unix_ms).date();
        bars.day = date == QDate::currentDate() ? "Today" : date.toString("ddd");
        bars.isToday = date == QDate::currentDate();
        bars.segments = {
            {day.idle_ms / static_cast<double>(kHourMs), QColor("#2A2D38")},
            {day.distraction_ms / static_cast<double>(kHourMs), QColor("#F59E0B")},
            {(day.active_ms - day.focused_ms - day.distraction_ms) / static_cast<double>(kHourMs),
             QColor("#7B8CDE")},
            {day.focused_ms / static_cast<double>(kHourMs), QColor("#00DFA2")},
        };
        chartDays.push_back(std::move(bars));
    }
    weeklyChart_->setDays(std::move(chartDays));

    const auto& profile = snapshot.profile;
    workstylePeak_->setText(profile.peak_half_hour < 0
                                ? "Not enough data"
                                : QString("%1:%2 – %3:%4")
                                      .arg(profile.peak_half_hour / 2, 2, 10, QChar('0'))
                                      .arg((profile.peak_half_hour % 2) * 30, 2, 10, QChar('0'))
                                      .arg(profile.peak_half_hour / 2 + 1, 2, 10, QChar('0'))
                                      .arg((profile.peak_half_hour % 2) * 30, 2, 10, QChar('0')));
    workstyleType_->setText(QString::fromStdString(profile.focus_pattern));
    workstyleDayLength_->setText(duration(snapshot.metrics.workday_elapsed_ms));
    const auto deepRatio =
        snapshot.metrics.active_ms > 0
            ? snapshot.metrics.deep_work_ms * 100 / snapshot.metrics.active_ms
            : 0;
    workstyleDeepRatio_->setText(QString("%1%").arg(deepRatio));

    const storage::DateRange historyRange{localDayStart(13), localDayEnd()};
    const auto history = activity_.dailyHistory(historyRange);
    historyTable_->setRowCount(static_cast<int>(history.size()));
    for (int row = 0; row < historyTable_->rowCount(); ++row) {
        const auto& day = history[static_cast<std::size_t>(row)];
        const auto date = QDateTime::fromMSecsSinceEpoch(day.day_start_unix_ms).date();
        const std::array<QString, 6> values{
            date.toString("ddd, MMM d"), duration(day.active_ms), duration(day.focused_ms),
            duration(day.deep_work_ms), duration(day.distraction_ms),
            QString::number(day.context_switch_count)};
        for (int column = 0; column < static_cast<int>(values.size()); ++column) {
            historyTable_->setItem(row, column, new QTableWidgetItem(values[column]));
        }
    }
}

void TrendsPage::redrawCharts() {
    weeklyChart_->update();
}

} // namespace activityos::ui
