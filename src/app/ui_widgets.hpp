#pragma once

#include <QColor>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>

namespace activityos::ui {

struct TimelineBlock {
    double startHour{};
    double endHour{};
    QString type;
    QString label;
    QString tooltip;
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

class DayTimelineCanvas;

class DayTimelineWidget final : public QWidget {
public:
    explicit DayTimelineWidget(QWidget* parent = nullptr);

    void setRange(double startHour, double endHour);
    void setBlocks(std::vector<TimelineBlock> blocks);
    void setNowHour(double nowHour);

protected:
    void resizeEvent(QResizeEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    void syncCanvasWidth();

    QScrollArea* scroll_{};
    DayTimelineCanvas* canvas_{};
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
