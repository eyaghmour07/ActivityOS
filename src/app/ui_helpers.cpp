#include "ui_helpers.hpp"

#include <QAbstractItemView>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QSizePolicy>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace activityos::ui {

QString duration(std::int64_t milliseconds) {
    const auto totalMinutes = std::max<std::int64_t>(0, milliseconds) / kMinuteMs;
    const auto hours = totalMinutes / 60;
    const auto minutes = totalMinutes % 60;
    return hours > 0 ? QStringLiteral("%1h %2m").arg(hours).arg(minutes)
                     : QStringLiteral("%1m").arg(minutes);
}

QString percent(double value) {
    return QString::number(value * 100.0, 'f', 0) + "%";
}

QLabel* monoCaption(const QString& text) {
    auto* label = new QLabel(text);
    label->setObjectName("monoCaption");
    return label;
}

QLabel* titleLabel(const QString& text) {
    auto* label = new QLabel(text);
    label->setObjectName("pageTitle");
    return label;
}

void equalizeColumns(QGridLayout* grid) {
    for (int column = 0; column < grid->columnCount(); ++column) {
        grid->setColumnStretch(column, 1);
    }
}

QFrame* metricCard(const QString& label, QLabel*& value, QLabel*& sub, bool accent) {
    auto* frame = new QFrame;
    frame->setObjectName(accent ? "metricCardAccent" : "metricCard");
    frame->setMinimumHeight(118);
    frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);
    auto* caption = monoCaption(label);
    caption->setWordWrap(true);
    layout->addWidget(caption);
    value = new QLabel("—");
    value->setObjectName("metricValue");
    value->setWordWrap(true);
    value->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    value->setMinimumHeight(34);
    layout->addWidget(value);
    sub = new QLabel;
    sub->setObjectName("metricSub");
    sub->setWordWrap(true);
    layout->addWidget(sub);
    layout->addStretch();
    return frame;
}

QFrame* sectionCard(QWidget* content) {
    auto* frame = new QFrame;
    frame->setObjectName("sectionCard");
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(14);
    layout->addWidget(content);
    return frame;
}

QWidget* wrapSection(const QString& title, QWidget* body) {
    auto* container = new QWidget;
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    if (!title.isEmpty()) layout->addWidget(monoCaption(title));
    layout->addWidget(body);
    return sectionCard(container);
}

QWidget* settingsRow(const QString& title, const QString& description, QWidget* control) {
    auto* row = new QFrame;
    row->setObjectName("settingsRow");
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(16);
    auto* text = new QVBoxLayout;
    text->setSpacing(4);
    auto* heading = new QLabel(title);
    heading->setObjectName("settingsTitle");
    heading->setWordWrap(true);
    auto* detail = new QLabel(description);
    detail->setObjectName("activeApp");
    detail->setWordWrap(true);
    text->addWidget(heading);
    text->addWidget(detail);
    layout->addLayout(text, 1);
    control->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    layout->addWidget(control, 0, Qt::AlignVCenter);
    return row;
}

QLabel* settingsSection(const QString& title) {
    auto* label = monoCaption(title);
    label->setObjectName("settingsSection");
    label->setContentsMargins(20, 14, 20, 10);
    return label;
}

void addPageHeader(QVBoxLayout* layout, const QString& eyebrow, const QString& title,
                   QWidget* trailing) {
    auto* row = new QHBoxLayout;
    row->setSpacing(12);
    auto* textColumn = new QVBoxLayout;
    textColumn->setSpacing(4);
    textColumn->addWidget(monoCaption(eyebrow));
    textColumn->addWidget(titleLabel(title));
    row->addLayout(textColumn, 1);
    if (trailing) row->addWidget(trailing, 0, Qt::AlignTop);
    layout->addLayout(row);
}

QTableWidget* table(const QStringList& headers) {
    auto* widget = new QTableWidget;
    widget->setColumnCount(headers.size());
    widget->setHorizontalHeaderLabels(headers);
    widget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    widget->verticalHeader()->hide();
    widget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    widget->setSelectionBehavior(QAbstractItemView::SelectRows);
    widget->setSelectionMode(QAbstractItemView::SingleSelection);
    widget->setAlternatingRowColors(true);
    widget->setShowGrid(false);
    widget->setFocusPolicy(Qt::NoFocus);
    widget->verticalHeader()->setDefaultSectionSize(42);
    widget->horizontalHeader()->setMinimumHeight(44);
    return widget;
}

std::int64_t localDayStart(int daysAgo) {
    return QDateTime(QDate::currentDate().addDays(-daysAgo), QTime(0, 0))
        .toMSecsSinceEpoch();
}

std::int64_t localDayEnd() {
    return localDayStart(-1);
}

AnalyticsConfig localAnalyticsConfig() {
    AnalyticsConfig config;
    // Looked up per timestamp, so a daylight-saving change while the app is
    // running is picked up without a restart.
    config.utc_offset_minutes_at = [](UnixMillis unixMs) {
        return QDateTime::fromMSecsSinceEpoch(unixMs).offsetFromUtc() / 60;
    };
    return config;
}

storage::DateRange todayRange() {
    return {localDayStart(), localDayEnd()};
}

QString insightPrefix(Insight::Kind kind) {
    switch (kind) {
    case Insight::Kind::Positive: return "Positive";
    case Insight::Kind::Warning: return "Attention";
    case Insight::Kind::Recommendation: return "Try";
    case Insight::Kind::Neutral: return "Note";
    }
    return "Note";
}

QString clockTime(qint64 unixMs) {
    return QDateTime::fromMSecsSinceEpoch(unixMs).toString("HH:mm");
}

} // namespace activityos::ui
