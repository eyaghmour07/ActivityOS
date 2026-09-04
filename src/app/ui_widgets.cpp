#include "ui_widgets.hpp"

#include <QHBoxLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QProgressBar>
#include <QScrollArea>
#include <QSizePolicy>

namespace activityos::ui {
namespace {

QColor timelineColor(const QString& type) {
    if (type == "deep") return QColor("#00DFA2");
    if (type == "shallow") return QColor("#7B8CDE");
    if (type == "comms") return QColor("#F59E0B");
    if (type == "distraction") return QColor("#EF4444");
    return QColor("#2A2D38");
}

} // namespace

DayTimelineWidget::DayTimelineWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(40);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void DayTimelineWidget::setRange(double startHour, double endHour) {
    startHour_ = startHour;
    endHour_ = endHour;
    update();
}

void DayTimelineWidget::setBlocks(std::vector<TimelineBlock> blocks) {
    blocks_ = std::move(blocks);
    update();
}

QSize DayTimelineWidget::sizeHint() const {
    return {640, 40};
}

void DayTimelineWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const auto span = std::max(0.5, endHour_ - startHour_);
    const int gap = 2;
    for (const auto& block : blocks_) {
        const auto left = ((block.startHour - startHour_) / span) * width();
        const auto blockWidth =
            ((block.endHour - block.startHour) / span) * width() - gap;
        if (blockWidth <= 0) continue;
        painter.setBrush(timelineColor(block.type));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(QRectF(left, 0, blockWidth, height()), 3, 3);
    }
}

AppBreakdownWidget::AppBreakdownWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
}

void AppBreakdownWidget::setRows(std::vector<AppUsageRow> rows) {
    rows_ = std::move(rows);
    rebuild();
}

void AppBreakdownWidget::rebuild() {
    if (auto* old = layout()) {
        QLayoutItem* item;
        while ((item = old->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete old;
    }
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    for (const auto& row : rows_) {
        auto* line = new QHBoxLayout;
        line->setSpacing(12);
        auto* name = new QLabel(row.name);
        name->setFixedWidth(112);
        name->setObjectName("appBreakdownName");
        auto* bar = new QProgressBar;
        bar->setRange(0, 100);
        bar->setValue(row.pct);
        bar->setTextVisible(false);
        bar->setFixedHeight(8);
        bar->setObjectName("appBreakdownBar");
        bar->setStyleSheet(QString(
            "QProgressBar { background: #161820; border: 0; border-radius: 4px; }"
            "QProgressBar::chunk { background: %1; border-radius: 4px; }")
                               .arg(row.color.name()));
        auto* minutes = new QLabel(QString("%1m").arg(row.minutes));
        minutes->setObjectName("metricSub");
        minutes->setFixedWidth(40);
        minutes->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        line->addWidget(name);
        line->addWidget(bar, 1);
        line->addWidget(minutes);
        layout->addLayout(line);
    }
    layout->addStretch();
}

WeeklyChartWidget::WeeklyChartWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(192);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void WeeklyChartWidget::setDays(std::vector<WeekDayBars> days) {
    days_ = std::move(days);
    update();
}

QSize WeeklyChartWidget::sizeHint() const {
    return {640, 192};
}

void WeeklyChartWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    if (days_.empty()) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), Qt::transparent);

    double maxTotal = 0.0;
    for (const auto& day : days_) {
        double total = 0.0;
        for (const auto& segment : day.segments) total += segment.first;
        maxTotal = std::max(maxTotal, total);
    }
    if (maxTotal <= 0.0) maxTotal = 1.0;

    const int bottomLabel = 24;
    const int chartHeight = height() - bottomLabel;
    const int gap = 12;
    const int barWidth = std::max(16, (width() - gap * (static_cast<int>(days_.size()) - 1)) /
                                            static_cast<int>(days_.size()));

    for (int i = 0; i < static_cast<int>(days_.size()); ++i) {
        const auto& day = days_[static_cast<std::size_t>(i)];
        const int x = i * (barWidth + gap);
        int y = chartHeight;
        for (const auto& segment : day.segments) {
            const int segmentHeight =
                static_cast<int>((segment.first / maxTotal) * chartHeight * 0.92);
            if (segmentHeight <= 0) continue;
            y -= segmentHeight;
            QColor color = segment.second;
            if (!day.isToday && color == QColor("#00DFA2")) {
                color.setAlpha(68);
            }
            painter.setBrush(color);
            painter.setPen(Qt::NoPen);
            painter.drawRect(x, y, barWidth, segmentHeight);
        }

        painter.setPen(day.isToday ? QColor("#00DFA2") : QColor("#5C6478"));
        QFont font = painter.font();
        font.setPointSize(9);
        painter.setFont(font);
        const auto label = day.day.left(3);
        painter.drawText(QRect(x, chartHeight + 4, barWidth, bottomLabel),
                         Qt::AlignHCenter | Qt::AlignTop, label);
    }
}

QWidget* makeRecordingIndicator(QLabel*& statusLabel) {
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    auto* dot = new QLabel;
    dot->setFixedSize(8, 8);
    dot->setObjectName("recordingDot");
    statusLabel = new QLabel("RECORDING");
    statusLabel->setObjectName("recordingLabel");
    layout->addWidget(dot, 0, Qt::AlignVCenter);
    layout->addWidget(statusLabel, 0, Qt::AlignVCenter);
    return row;
}

QWidget* makePageColumn(QVBoxLayout*& layout, int maxWidth) {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setObjectName("pageScroll");
    scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* inner = new QWidget;
    inner->setObjectName("pageContent");
    inner->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    if (maxWidth > 0) inner->setMaximumWidth(maxWidth);
    layout = new QVBoxLayout(inner);
    layout->setContentsMargins(36, 28, 36, 28);
    layout->setSpacing(22);

    scroll->setWidget(inner);
    return scroll;
}

} // namespace activityos::ui
