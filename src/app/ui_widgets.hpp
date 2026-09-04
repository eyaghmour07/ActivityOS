#pragma once

#include <QColor>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>

namespace activityos::ui {

struct TimelineBlock {
    double startHour{};
    double endHour{};
    QString type;
    QString label;
};

struct AppUsageRow {
    QString name;
    int minutes{};
    int pct{};
    QColor color;
};

struct WeekDayBars {
    QString day;
    bool isToday{};
    std::vector<std::pair<double, QColor>> segments;
};

class DayTimelineWidget final : public QWidget {
public:
    explicit DayTimelineWidget(QWidget* parent = nullptr);

    void setRange(double startHour, double endHour);
    void setBlocks(std::vector<TimelineBlock> blocks);

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize sizeHint() const override;

private:
    double startHour_{8.0};
    double endHour_{18.0};
    std::vector<TimelineBlock> blocks_;
};

class AppBreakdownWidget final : public QWidget {
public:
    explicit AppBreakdownWidget(QWidget* parent = nullptr);

    void setRows(std::vector<AppUsageRow> rows);

private:
    void rebuild();

    std::vector<AppUsageRow> rows_;
};

class WeeklyChartWidget final : public QWidget {
public:
    explicit WeeklyChartWidget(QWidget* parent = nullptr);

    void setDays(std::vector<WeekDayBars> days);

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize sizeHint() const override;

private:
    std::vector<WeekDayBars> days_;
};

QWidget* makeRecordingIndicator(QLabel*& statusLabel);
QWidget* makePageColumn(QVBoxLayout*& layout, int maxWidth = 0);

} // namespace activityos::ui
