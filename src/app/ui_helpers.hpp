#pragma once

#include "activityos/services.hpp"

#include <QString>
#include <QStringList>

#include <cstdint>

class QFrame;
class QGridLayout;
class QLabel;
class QTableWidget;
class QVBoxLayout;
class QWidget;

// Formatting, local-time and widget-building helpers shared by the dashboard pages.
namespace activityos::ui {

inline constexpr std::int64_t kMinuteMs = 60 * 1000;
inline constexpr std::int64_t kHourMs = 60 * kMinuteMs;
inline constexpr std::int64_t kDayMs = 24 * kHourMs;

QString duration(std::int64_t milliseconds);
QString percent(double value);
QString clockTime(qint64 unixMs);
QString insightPrefix(Insight::Kind kind);

QLabel* monoCaption(const QString& text);
QLabel* titleLabel(const QString& text);
void equalizeColumns(QGridLayout* grid);
QFrame* metricCard(const QString& label, QLabel*& value, QLabel*& sub, bool accent = false);
QFrame* sectionCard(QWidget* content);
QWidget* wrapSection(const QString& title, QWidget* body);
QWidget* settingsRow(const QString& title, const QString& description, QWidget* control);
QLabel* settingsSection(const QString& title);
void addPageHeader(QVBoxLayout* layout, const QString& eyebrow, const QString& title,
                   QWidget* trailing = nullptr);
QTableWidget* table(const QStringList& headers);

// Local midnight `daysAgo` calendar days before today; negative values are in the future.
std::int64_t localDayStart(int daysAgo = 0);
std::int64_t localDayEnd();
storage::DateRange todayRange();
AnalyticsConfig localAnalyticsConfig();

} // namespace activityos::ui
