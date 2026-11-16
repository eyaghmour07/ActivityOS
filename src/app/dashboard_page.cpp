#include "dashboard_page.hpp"

#include <QFrame>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace activityos::ui {

DashboardPage::DashboardPage(QWidget* parent) : QScrollArea(parent) {
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setObjectName("pageScroll");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* inner = new QWidget;
    inner->setObjectName("pageContent");
    inner->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    content_layout_ = new QVBoxLayout(inner);
    content_layout_->setContentsMargins(36, 28, 36, 28);
    content_layout_->setSpacing(22);

    setWidget(inner);
}

} // namespace activityos::ui
