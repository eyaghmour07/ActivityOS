#include "focus_page.hpp"

#include "ui_helpers.hpp"

#include <QGridLayout>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

namespace activityos::ui {
namespace {

int sessionDurationMs(const Session& session) {
    if (session.end_unix_ms <= session.start_unix_ms) return 0;
    const auto elapsed = session.end_unix_ms - session.start_unix_ms;
    if (session.active_duration_ms <= 0) return static_cast<int>(elapsed);
    return static_cast<int>(std::min(session.active_duration_ms, elapsed));
}

int sessionScore(const Session& session) {
    int score = 68;
    const auto minutes = session.active_duration_ms / kMinuteMs;
    if (minutes >= 120) score += 22;
    else if (minutes >= 90) score += 16;
    else if (minutes >= 60) score += 10;
    else if (minutes >= 30) score += 6;
    if (session.productive && !session.distraction) score += 8;
    if (session.distraction) score -= 18;
    return std::clamp(score, 0, 100);
}

} // namespace

FocusPage::FocusPage(ActivityService& activity, QWidget* parent)
    : DashboardPage(parent), activity_(activity) {
    auto* layout = contentLayout();
    addPageHeader(layout, "Deep Work", "Focus Sessions");

    auto* cards = new QGridLayout;
    cards->setHorizontalSpacing(12);
    cards->addWidget(metricCard("Total Focus", focusTotalValue_, focusTotalSub_, true), 0, 0);
    cards->addWidget(metricCard("Longest Session", focusLongestValue_, focusLongestSub_), 0, 1);
    cards->addWidget(metricCard("Avg Score", focusAvgScoreValue_, focusAvgScoreSub_), 0, 2);
    cards->addWidget(metricCard("Flow Entries", focusFlowValue_, focusFlowSub_, true), 0, 3);
    equalizeColumns(cards);
    layout->addLayout(cards);

    focusSessionsTable_ = table({"Start", "End", "Duration", "Primary App", "Score"});
    layout->addWidget(wrapSection("Session Log", focusSessionsTable_), 1);
}

void FocusPage::refresh(const DashboardSnapshot& snapshot) {
    focusTotalValue_->setText(duration(snapshot.metrics.focused_ms));
    focusTotalSub_->setText("today");
    focusLongestValue_->setText(duration(snapshot.metrics.longest_session_ms));
    focusLongestSub_->setText("longest block");

    const auto daySessions = activity_.sessions(todayRange());
    const auto switches = activity_.switches(todayRange());
    AnalyticsEngine engine(localAnalyticsConfig());
    const auto focusBlocks = engine.focusSessions(daySessions, switches);
    int totalScore = 0;
    int flowEntries = 0;
    for (const auto& block : focusBlocks) {
        const auto score = sessionScore(block);
        totalScore += score;
        if (score >= 85) ++flowEntries;
    }
    focusAvgScoreValue_->setText(focusBlocks.empty()
                                     ? "—"
                                     : QString("%1/100").arg(totalScore /
                                                             static_cast<int>(focusBlocks.size())));
    focusAvgScoreSub_->setText(QString("across %1 sessions").arg(focusBlocks.size()));
    focusFlowValue_->setText(QString::number(flowEntries));
    focusFlowSub_->setText("score ≥ 85");

    focusSessionsTable_->setRowCount(static_cast<int>(focusBlocks.size()));
    for (int row = 0; row < focusSessionsTable_->rowCount(); ++row) {
        const auto& block = focusBlocks[static_cast<std::size_t>(row)];
        const std::array<QString, 5> values{
            clockTime(block.start_unix_ms), clockTime(block.end_unix_ms),
            duration(sessionDurationMs(block)), QString::fromStdString(block.app),
            QString::number(sessionScore(block))};
        for (int column = 0; column < static_cast<int>(values.size()); ++column) {
            focusSessionsTable_->setItem(row, column, new QTableWidgetItem(values[column]));
        }
    }
}

} // namespace activityos::ui
