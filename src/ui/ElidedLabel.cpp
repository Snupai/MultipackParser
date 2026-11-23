/**
 * @file ElidedLabel.cpp
 * @brief One-line label that shortens its text with "..." instead of growing
 */
#include "multipack/ui/ElidedLabel.h"

#include <QPainter>
#include <QStyle>

namespace multipack {
namespace ui {

ElidedLabel::ElidedLabel(QWidget* parent)
    : QLabel(parent)
{
    setTextFormat(Qt::PlainText);
}

QSize ElidedLabel::minimumSizeHint() const
{
    // The text never forces a width; the layout decides.
    return QSize(0, QLabel::minimumSizeHint().height());
}

void ElidedLabel::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    const QRect area = contentsRect();
    const QString full = text();
    const QString shown = fontMetrics().elidedText(full, Qt::ElideRight, area.width());
    setToolTip(shown == full ? QString() : full);

    QPainter painter(this);
    style()->drawItemText(&painter, area, static_cast<int>(alignment()), palette(), isEnabled(),
                          shown, foregroundRole());
}

} // namespace ui
} // namespace multipack
