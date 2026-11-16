#include "apps_page.hpp"

#include "ui_helpers.hpp"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <numeric>
#include <unordered_map>
#include <utility>

namespace activityos::ui {

AppsPage::AppsPage(ActivityService& activity, QWidget* parent)
    : DashboardPage(parent), activity_(activity) {
    auto* layout = contentLayout();
    addPageHeader(layout, "Usage", "Applications");

    auto* help = new QLabel(
        "Chrome tabs are classified from the active site URL when macOS allows ActivityOS "
        "to control Google Chrome. Other browsers fall back to visible titles.");
    help->setObjectName("activeApp");
    help->setWordWrap(true);
    layout->addWidget(help);

    auto* addRule = new QPushButton("Add Classification Rule");
    auto* removeRule = new QPushButton("Delete Selected Rule");
    connect(addRule, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const auto app = QInputDialog::getText(window(), "Classification Rule",
                                               "Application name contains:",
                                               QLineEdit::Normal, {}, &ok);
        if (!ok || app.trimmed().isEmpty()) return;
        const auto title = QInputDialog::getText(
            window(), "Classification Rule",
            "Optional window-title text (useful for browser tabs):",
            QLineEdit::Normal, {}, &ok);
        if (!ok) return;
        const QStringList categories{"Coding", "Research", "Communication", "Planning",
                                     "Administration", "Work", "Entertainment", "General",
                                     "Other"};
        const auto category = QInputDialog::getItem(window(), "Classification Rule", "Category:",
                                                    categories, 0, true, &ok);
        if (!ok) return;
        const bool distraction =
            QMessageBox::question(window(), "Classification Rule",
                                  "Should this rule count as a potential distraction?",
                                  QMessageBox::Yes | QMessageBox::No,
                                  category == "Entertainment" ? QMessageBox::Yes
                                                              : QMessageBox::No) ==
            QMessageBox::Yes;
        storage::ClassificationRule rule;
        rule.application_pattern = app.toStdString();
        rule.title_pattern = title.toStdString();
        rule.category = category.toStdString();
        rule.is_distraction = distraction;
        rule.priority = 100;
        rule.created_at = rule.updated_at = unixMillisecondsNow();
        activity_.saveRule(rule);
        reload();
    });
    connect(removeRule, &QPushButton::clicked, this, [this] {
        const int row = rulesTable_->currentRow();
        const auto rules = activity_.rules();
        if (row >= 0 && static_cast<std::size_t>(row) < rules.size()) {
            activity_.deleteRule(rules[static_cast<std::size_t>(row)].id);
            reload();
        }
    });
    auto* actions = new QHBoxLayout;
    actions->addWidget(addRule);
    actions->addWidget(removeRule);
    actions->addStretch();
    layout->addLayout(actions);

    appsTable_ = table({"Application", "Category", "Time Today", "Share", "Status"});
    layout->addWidget(wrapSection("Application Usage", appsTable_), 1);
    rulesTable_ = table({"Application Pattern", "Title Pattern", "Category", "Distraction",
                         "Priority"});
    layout->addWidget(wrapSection("Classification Rules", rulesTable_), 1);
}

void AppsPage::refresh(const DashboardSnapshot&) {
    reload();
}

void AppsPage::reload() {
    const auto range = todayRange();
    const auto daySessions = activity_.sessions(range);
    std::unordered_map<std::string, std::int64_t> appMinutes;
    std::unordered_map<std::string, Session> appSample;
    for (const auto& session : daySessions) {
        appMinutes[session.app] += session.active_duration_ms;
        appSample[session.app] = session;
    }
    const auto totalActive = std::accumulate(
        appMinutes.begin(), appMinutes.end(), std::int64_t{0},
        [](std::int64_t sum, const auto& item) { return sum + item.second; });

    std::vector<std::pair<std::string, std::int64_t>> ranked(appMinutes.begin(), appMinutes.end());
    std::sort(ranked.begin(), ranked.end(),
              [](const auto& left, const auto& right) { return left.second > right.second; });

    appsTable_->setRowCount(static_cast<int>(ranked.size()));
    for (int row = 0; row < appsTable_->rowCount(); ++row) {
        const auto& [name, ms] = ranked[static_cast<std::size_t>(row)];
        const auto& sample = appSample.at(name);
        const auto minutes = (ms + kMinuteMs / 2) / kMinuteMs;
        const auto share = totalActive > 0 ? (ms * 100 / totalActive) : 0;
        const QString status =
            sample.distraction ? "distraction"
                               : (sample.productive ? "productive" : "neutral");
        const std::array<QString, 5> values{
            QString::fromStdString(name), QString::fromStdString(sample.category),
            QString("%1m").arg(minutes), QString("%1%").arg(share), status};
        for (int column = 0; column < static_cast<int>(values.size()); ++column) {
            appsTable_->setItem(row, column, new QTableWidgetItem(values[column]));
        }
    }

    const auto rules = activity_.rules();
    rulesTable_->setRowCount(static_cast<int>(rules.size()));
    for (int row = 0; row < rulesTable_->rowCount(); ++row) {
        const auto& rule = rules[static_cast<std::size_t>(row)];
        const std::array<QString, 5> values{
            QString::fromStdString(rule.application_pattern),
            QString::fromStdString(rule.title_pattern), QString::fromStdString(rule.category),
            rule.is_distraction ? "Yes" : "No", QString::number(rule.priority)};
        for (int column = 0; column < static_cast<int>(values.size()); ++column) {
            rulesTable_->setItem(row, column, new QTableWidgetItem(values[column]));
        }
    }
}

} // namespace activityos::ui
