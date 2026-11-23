/**
 * @file ScaledImageLabel.cpp
 * @brief Image label that scales its pixmap to the space it gets, keeping the aspect ratio
 */
#include "multipack/ui/ScaledImageLabel.h"

#include <QPainter>
#include <QPixmap>

namespace multipack {
namespace ui {

ScaledImageLabel::ScaledImageLabel(QWidget* parent)
    : QLabel(parent)
{
}

QSize ScaledImageLabel::sizeHint() const
{
    // Preferred: the largest size the form allows, else the image as it is.
    const QSize limit = maximumSize();
    if (limit.width() < QWIDGETSIZE_MAX && limit.height() < QWIDGETSIZE_MAX) {
        return limit;
    }
    return pixmap().size();
}

QSize ScaledImageLabel::minimumSizeHint() const
{
    return QSize(0, 0);
}

void ScaledImageLabel::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    const QPixmap image = pixmap();
    if (image.isNull()) {
        return;
    }
    const QRect area = contentsRect();
    const QSize size = image.size().scaled(area.size(), Qt::KeepAspectRatio);
    const QRect target(area.left() + (area.width() - size.width()) / 2,
                       area.top() + (area.height() - size.height()) / 2, size.width(), size.height());

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawPixmap(target, image);
}

} // namespace ui
} // namespace multipack
