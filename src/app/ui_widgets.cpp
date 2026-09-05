#include "ui_widgets.hpp"

#include <QEvent>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QPolygonF>
#include <QProgressBar>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QToolTip>

#include <algorithm>
#include <cmath>

namespace activityos::ui {
namespace {

QColor timelineColor(const QString& type) {
    if (type == "work" || type == "shallow") return QColor("#7B8CDE");
    if (type == "comms") return QColor("#E8A13A");
    if (type == "distraction") return QColor("#E05A5A");
    if (type == "deep") return QColor("#7B8CDE");
    return QColor("#2A2D38");
}

QString formatTimelineHour(double hour) {
    hour = std::clamp(hour, 0.0, 24.0);
    auto totalMinutes = static_cast<int>(std::lround(hour * 60.0));
    if (totalMinutes >= 24 * 60) totalMinutes = 24 * 60;
    const int hours = totalMinutes / 60;
    const int minutes = totalMinutes % 60;
    return QString("%1:%2").arg(hours, 2, 10, QChar('0')).arg(minutes, 2, 10, QChar('0'));
}

int timelineTickStep(double span) {
    if (span <= 6.0) return 1;
    if (span <= 12.0) return 2;
    return 3;
}

} // namespace

constexpr int kTimelineTrackHeight = 52;
constexpr int kTimelineLabelHeight = 24;
constexpr int kTimelineHeight = kTimelineTrackHeight + kTimelineLabelHeight;
constexpr int kTimelinePxPerHour = 64;
constexpr double kTimelineFillSpanHours = 12.0;

class DayTimelineCanvas final : public QWidget {
public:
    explicit DayTimelineCanvas(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(kTimelineHeight);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setMouseTracking(true);
    }

    void setRange(double startHour, double endHour) {
        startHour_ = startHour;
        endHour_ = std::max(endHour, startHour + 0.5);
        update();
    }

    void setBlocks(std::vector<TimelineBlock> blocks) {
        blocks_ = std::move(blocks);
        update();
    }

    void setNowHour(double nowHour) {
        nowHour_ = nowHour;
        update();
    }

    double span() const { return std::max(0.5, endHour_ - startHour_); }

    int preferredWidth(int viewportWidth) const {
        const auto hours = span();
        if (hours <= kTimelineFillSpanHours) return std::max(240, viewportWidth);
        return std::max(viewportWidth, static_cast<int>(std::ceil(hours * kTimelinePxPerHour)));
    }

    QSize sizeHint() const override { return {640, kTimelineHeight}; }

protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QRectF track(0, 0, width(), kTimelineTrackHeight);
        QPainterPath trackClip;
        trackClip.addRoundedRect(track, 6, 6);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#161820"));
        painter.drawPath(trackClip);

        const auto hours = span();
        const double pxPerHour = width() / hours;

        const int step = timelineTickStep(hours);
        int tick = static_cast<int>(std::ceil(startHour_));
        if (tick % step != 0) tick += step - (tick % step);
        QFont tickFont = painter.font();
        tickFont.setPixelSize(10);
        tickFont.setWeight(QFont::DemiBold);
        painter.setFont(tickFont);

        for (; tick <= static_cast<int>(std::floor(endHour_)); tick += step) {
            const double x = (tick - startHour_) * pxPerHour;
            painter.setPen(QPen(QColor(255, 255, 255, 18), 1));
            painter.drawLine(QPointF(x, 6), QPointF(x, kTimelineTrackHeight - 6));
            painter.setPen(QColor("#5C6478"));
            const auto label = formatTimelineHour(tick);
            const int labelWidth = 48;
            painter.drawText(QRectF(x - labelWidth / 2.0, kTimelineTrackHeight + 2, labelWidth,
                                    kTimelineLabelHeight - 2),
                             Qt::AlignHCenter | Qt::AlignTop, label);
        }

        painter.save();
        painter.setClipPath(trackClip);
        painter.setPen(Qt::NoPen);
        const QRectF ribbon(0, 10, width(), kTimelineTrackHeight - 20);
        for (const auto& block : blocks_) {
            if (block.endHour <= startHour_ || block.startHour >= endHour_) continue;
            const double left = (block.startHour - startHour_) * pxPerHour;
            const double blockWidth = (block.endHour - block.startHour) * pxPerHour + 0.5;
            if (blockWidth < 2.0) continue;
            const QRectF rect(left, ribbon.top(), blockWidth, ribbon.height());
            QLinearGradient fill(rect.topLeft(), rect.bottomLeft());
            const auto color = timelineColor(block.type);
            fill.setColorAt(0.0, color.lighter(118));
            fill.setColorAt(1.0, color.darker(118));
            painter.setBrush(fill);
            painter.drawRect(rect);
        }
        painter.restore();

        if (nowHour_ >= startHour_ && nowHour_ <= endHour_) {
            const double x = (nowHour_ - startHour_) * pxPerHour;
            painter.setPen(QPen(QColor("#00DFA2"), 1.5));
            painter.drawLine(QPointF(x, 2), QPointF(x, kTimelineTrackHeight - 2));
            painter.setBrush(QColor("#00DFA2"));
            painter.setPen(Qt::NoPen);
            QPolygonF marker;
            marker << QPointF(x, 0) << QPointF(x - 4, 6) << QPointF(x + 4, 6);
            painter.drawPolygon(marker);
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        const auto* block = blockAt(event->position().x());
        if (block && !block->tooltip.isEmpty()) {
            QToolTip::showText(event->globalPosition().toPoint(), block->tooltip, this);
            setCursor(Qt::PointingHandCursor);
        } else {
            QToolTip::hideText();
            unsetCursor();
        }
        QWidget::mouseMoveEvent(event);
    }

    void leaveEvent(QEvent* event) override {
        QToolTip::hideText();
        unsetCursor();
        QWidget::leaveEvent(event);
    }

private:
    const TimelineBlock* blockAt(double x) const {
        const auto hours = span();
        if (hours <= 0.0 || width() <= 0) return nullptr;
        const double hour = startHour_ + (x / width()) * hours;
        for (const auto& block : blocks_) {
            if (hour >= block.startHour && hour <= block.endHour) return &block;
        }
        return nullptr;
    }

    double startHour_{8.0};
    double endHour_{12.0};
    double nowHour_{-1.0};
    std::vector<TimelineBlock> blocks_;
};

DayTimelineWidget::DayTimelineWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(kTimelineHeight + 10);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    scroll_ = new QScrollArea;
    scroll_->setObjectName("timelineScroll");
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setWidgetResizable(false);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setFixedHeight(kTimelineHeight + 10);

    canvas_ = new DayTimelineCanvas;
    scroll_->setWidget(canvas_);
    layout->addWidget(scroll_);
}

void DayTimelineWidget::setRange(double startHour, double endHour) {
    canvas_->setRange(startHour, endHour);
    syncCanvasWidth();
}

void DayTimelineWidget::setBlocks(std::vector<TimelineBlock> blocks) {
    canvas_->setBlocks(std::move(blocks));
}

void DayTimelineWidget::setNowHour(double nowHour) {
    canvas_->setNowHour(nowHour);
}

void DayTimelineWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    syncCanvasWidth();
}

void DayTimelineWidget::syncCanvasWidth() {
    const int viewportWidth = std::max(1, scroll_->viewport()->width());
    const int width = canvas_->preferredWidth(viewportWidth);
    canvas_->resize(width, kTimelineHeight);
}

QSize DayTimelineWidget::sizeHint() const {
    return {640, kTimelineHeight + 10};
}

QSize DayTimelineWidget::minimumSizeHint() const {
    return {240, kTimelineHeight + 10};
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

    constexpr int kNameWidth = 168;
    constexpr int kMinutesWidth = 48;

    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(14);
    grid->setColumnStretch(1, 1);
    grid->setColumnMinimumWidth(0, kNameWidth);
    grid->setColumnMinimumWidth(2, kMinutesWidth);

    int rowIndex = 0;
    for (const auto& row : rows_) {
        auto* name = new QLabel;
        name->setObjectName("appBreakdownName");
        name->setFixedWidth(kNameWidth);
        name->setWordWrap(false);
        name->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        name->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        QFont nameFont = name->font();
        nameFont.setPixelSize(14);
        nameFont.setWeight(QFont::Medium);
        name->setFont(nameFont);
        const QFontMetrics metrics(nameFont);
        name->setText(metrics.elidedText(row.name, Qt::ElideRight, kNameWidth));
        if (metrics.horizontalAdvance(row.name) > kNameWidth) {
            name->setToolTip(row.name);
        }

        auto* bar = new QProgressBar;
        bar->setRange(0, 100);
        bar->setValue(row.pct);
        bar->setTextVisible(false);
        bar->setFixedHeight(8);
        bar->setMinimumWidth(48);
        bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        bar->setObjectName("appBreakdownBar");
        bar->setStyleSheet(QString(
            "QProgressBar {"
            "  background: transparent;"
            "  border: 0;"
            "  border-radius: 4px;"
            "}"
            "QProgressBar::chunk {"
            "  background: %1;"
            "  border-radius: 4px;"
            "}")
                               .arg(row.color.name()));

        auto* minutes = new QLabel(QString("%1m").arg(row.minutes));
        minutes->setObjectName("appBreakdownMinutes");
        minutes->setFixedWidth(kMinutesWidth);
        minutes->setWordWrap(false);
        minutes->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        minutes->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

        grid->addWidget(name, rowIndex, 0, Qt::AlignVCenter);
        grid->addWidget(bar, rowIndex, 1, Qt::AlignVCenter);
        grid->addWidget(minutes, rowIndex, 2, Qt::AlignVCenter);
        ++rowIndex;
    }
    grid->setRowStretch(rowIndex, 1);
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
