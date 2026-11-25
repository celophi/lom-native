#include "window/backdrop.h"

#include "window/theme.h"

#include <QLinearGradient>
#include <QPainter>

namespace lom::launcher {

namespace {
constexpr int shade_red = 8;
constexpr int shade_green = 16;
constexpr int shade_blue = 30;
constexpr int top_shade = 150;
constexpr int bottom_shade = 175;
constexpr int dark_dim = 70;

}

Backdrop::Backdrop(QWidget* parent) : QWidget(parent), image_(Theme::background()) {
    setObjectName("backdrop");
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void Backdrop::resizeEvent(QResizeEvent* event) {
    scaled_ = QPixmap();
    QWidget::resizeEvent(event);
}

void Backdrop::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    const QRect area = rect();

    if (image_.isNull()) {
        QLinearGradient sky(0, 0, 0, area.height());
        sky.setColorAt(0.0, QColor("#3d7ec4"));
        sky.setColorAt(1.0, QColor("#16304d"));
        painter.fillRect(area, sky);
    } else {
        if (scaled_.isNull()) {
            const qreal ratio = devicePixelRatioF();
            scaled_ = image_.scaled(area.size() * ratio, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            scaled_.setDevicePixelRatio(ratio);
        }

        const QSizeF size = scaled_.deviceIndependentSize();
        painter.drawPixmap(QPointF((area.width() - size.width()) / 2, (area.height() - size.height()) / 2), scaled_);
    }

    QLinearGradient shade(0, 0, 0, area.height());
    shade.setColorAt(0.0, QColor(shade_red, shade_green, shade_blue, top_shade));
    shade.setColorAt(0.32, QColor(shade_red, shade_green, shade_blue, 0));
    shade.setColorAt(0.6, QColor(shade_red, shade_green, shade_blue, 0));
    shade.setColorAt(1.0, QColor(shade_red, shade_green, shade_blue, bottom_shade));
    painter.fillRect(area, shade);

    if (Theme::is_dark()) {
        painter.fillRect(area, QColor(0, 0, 0, dark_dim));
    }
}

}
