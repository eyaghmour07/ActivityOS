#include "goals_page.hpp"

#include "ui_helpers.hpp"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

namespace activityos::ui {

GoalsPage::GoalsPage(ActivityService& activity, storage::Database& database, QWidget* parent)
    : DashboardPage(parent), activity_(activity), database_(database) {
    auto* layout = contentLayout();
    addPageHeader(layout, "Productivity", "Goals & Experiments");

    auto* add = new QPushButton("New Goal");
    auto* remove = new QPushButton("Delete Selected");
    connect(add, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const auto name = QInputDialog::getText(window(), "New Goal", "Goal name:",
                                                QLineEdit::Normal, {}, &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        const QStringList metrics{"focus_time", "deep_work", "distraction_time",
                                  "context_switches", "switch_rate", "active_time"};
        const auto metric =
            QInputDialog::getItem(window(), "New Goal", "Metric:", metrics, 0, false, &ok);
        if (!ok) return;
        const double target = QInputDialog::getDouble(
            window(), "New Goal",
            metric.contains("time") || metric == "deep_work"
                ? "Target minutes:"
                : "Target value:",
            60, 0.01, 100000, 1, &ok);
        if (!ok) return;
        storage::Goal goal;
        goal.name = name.toStdString();
        goal.metric = metric.toStdString();
        goal.target_value =
            metric.contains("time") || metric == "deep_work" ? target * kMinuteMs : target;
        goal.period = "weekly";
        goal.created_at = unixMillisecondsNow();
        activity_.saveGoal(goal);
        reloadGoals();
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        const int row = goalsTable_->currentRow();
        const auto goals = database_.goals(false);
        if (row >= 0 && static_cast<std::size_t>(row) < goals.size()) {
            activity_.deleteGoal(goals[static_cast<std::size_t>(row)].id);
            reloadGoals();
        }
    });
    auto* actions = new QHBoxLayout;
    actions->addWidget(add);
    actions->addWidget(remove);
    actions->addStretch();
    layout->addLayout(actions);

    goalsCardsLayout_ = new QVBoxLayout;
    goalsCardsLayout_->setSpacing(12);
    auto* goalsHost = new QWidget;
    goalsHost->setLayout(goalsCardsLayout_);
    layout->addWidget(goalsHost);

    experimentCard_ = new QFrame;
    experimentCard_->setObjectName("sectionCard");
    auto* experimentLayout = new QVBoxLayout(experimentCard_);
    experimentLayout->setContentsMargins(20, 20, 20, 20);
    experimentLayout->setSpacing(12);
    experimentLayout->addWidget(monoCaption("Active Experiment"));
    experimentTitle_ = new QLabel("No active experiment");
    experimentTitle_->setObjectName("goalTitle");
    experimentTitle_->setWordWrap(true);
    experimentBody_ = new QLabel;
    experimentBody_->setObjectName("activeApp");
    experimentBody_->setWordWrap(true);
    experimentLayout->addWidget(experimentTitle_);
    experimentLayout->addWidget(experimentBody_);
    auto* experimentStats = new QGridLayout;
    experimentStats->setHorizontalSpacing(16);
    QLabel* unusedBefore = nullptr;
    QLabel* unusedAfter = nullptr;
    QLabel* unusedDelta = nullptr;
    experimentStats->addWidget(metricCard("Before", experimentBefore_, unusedBefore), 0, 0);
    experimentStats->addWidget(metricCard("After", experimentAfter_, unusedAfter, true), 0, 1);
    experimentStats->addWidget(metricCard("Change", experimentDelta_, unusedDelta, true), 0, 2);
    unusedBefore->setText("baseline");
    unusedAfter->setText("latest");
    unusedDelta->setText("vs baseline");
    equalizeColumns(experimentStats);
    experimentLayout->addLayout(experimentStats);
    layout->addWidget(experimentCard_);

    goalsTable_ = table({"Goal", "Metric", "Current", "Target", "Progress", "Status"});
    layout->addWidget(wrapSection("All Goals", goalsTable_), 1);
    experimentsTable_ = table({"Experiment", "Hypothesis", "Intervention", "Status", "Result"});
    experimentsTable_->hide();
}

void GoalsPage::refresh(const DashboardSnapshot&) {
    reloadGoals();
    reloadExperiment();
}

void GoalsPage::reloadGoals() {
    while (auto* item = goalsCardsLayout_->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    const storage::DateRange range{localDayStart(6), localDayEnd()};
    const auto goals = activity_.goalProgress(range);
    goalsTable_->setRowCount(static_cast<int>(goals.size()));
    for (int row = 0; row < goalsTable_->rowCount(); ++row) {
        const auto& goal = goals[static_cast<std::size_t>(row)];
        const bool timeMetric = goal.goal.metric.find("time") != std::string::npos ||
                                goal.goal.metric == "deep_work";
        const std::array<QString, 6> values{
            QString::fromStdString(goal.goal.name), QString::fromStdString(goal.goal.metric),
            timeMetric ? duration(static_cast<std::int64_t>(goal.current_value))
                       : QString::number(goal.current_value, 'f', 1),
            timeMetric ? duration(static_cast<std::int64_t>(goal.goal.target_value))
                       : QString::number(goal.goal.target_value, 'f', 1),
            percent(goal.progress), goal.achieved ? "achieved" : "on-track"};
        for (int column = 0; column < static_cast<int>(values.size()); ++column) {
            goalsTable_->setItem(row, column, new QTableWidgetItem(values[column]));
        }

        auto* card = new QFrame;
        card->setObjectName("sectionCard");
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(20, 18, 20, 18);
        cardLayout->setSpacing(10);
        auto* titleRow = new QHBoxLayout;
        auto* name = new QLabel(QString::fromStdString(goal.goal.name));
        name->setObjectName("goalTitle");
        name->setWordWrap(true);
        auto* badge = new QLabel(values.back());
        badge->setObjectName(goal.achieved ? "statusBadge" : "statusBadgeTrack");
        titleRow->addWidget(name, 1);
        titleRow->addWidget(badge, 0, Qt::AlignTop);
        cardLayout->addLayout(titleRow);
        auto* detail = new QLabel(QString("%1 of %2").arg(values[2], values[3]));
        detail->setObjectName("activeApp");
        cardLayout->addWidget(detail);
        auto* progress = new QProgressBar;
        progress->setRange(0, 100);
        progress->setValue(std::clamp(static_cast<int>(goal.progress * 100.0), 0, 100));
        progress->setTextVisible(false);
        cardLayout->addWidget(progress);
        auto* percentLabel = new QLabel(percent(goal.progress));
        percentLabel->setObjectName("metricSub");
        cardLayout->addWidget(percentLabel);
        goalsCardsLayout_->addWidget(card);
    }
    if (goals.empty()) {
        goalsCardsLayout_->addWidget(new QLabel("No goals yet. Add one to start tracking progress."));
    }
}

void GoalsPage::reloadExperiment() {
    const auto experiments = activity_.experimentResults();
    const auto active = std::find_if(experiments.begin(), experiments.end(), [](const auto& item) {
        return item.experiment.status == "active";
    });
    if (active == experiments.end()) {
        experimentTitle_->setText("No active experiment");
        experimentBody_->setText("Create an experiment to compare a baseline with an intervention.");
        experimentBefore_->setText("—");
        experimentAfter_->setText("—");
        experimentDelta_->setText("—");
        return;
    }
    experimentTitle_->setText(QString::fromStdString(active->experiment.name));
    experimentBody_->setText(QString::fromStdString(active->experiment.intervention));
    experimentBefore_->setText(active->experiment.baseline_value
                                   ? QString::number(*active->experiment.baseline_value, 'f', 0)
                                   : "—");
    experimentAfter_->setText(active->experiment.result_value
                                  ? QString::number(*active->experiment.result_value, 'f', 0)
                                  : "—");
    experimentDelta_->setText(
        (active->change_percent >= 0 ? "+" : "") +
        QString::number(active->change_percent, 'f', 0));
}

} // namespace activityos::ui
