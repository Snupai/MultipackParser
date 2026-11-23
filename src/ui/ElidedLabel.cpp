/**
 * @file ElidedLabel.cpp
 * @brief Label for a slot of limited width: shortens its text instead of growing
 */
#include "multipack/ui/ElidedLabel.h"

#include "multipack/ui/Theme.h"

#include <QColor>
#include <QFontMetrics>
#include <QPainter>
#include <QPalette>
#include <QStyle>

namespace multipack {
namespace ui {

ElidedLabel::ElidedLabel(QWidget* parent)
    : QLabel(parent)
{
    setTextFormat(Qt::PlainText);
}

void ElidedLabel::setDetailSeparator(const QString& separator)
{
    m_detailSeparator = separator;
    update();
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
    const int split = m_detailSeparator.isEmpty() ? -1 : full.lastIndexOf(m_detailSeparator);
    const int horizontal = static_cast<int>(alignment() & Qt::AlignHorizontal_Mask);

    QPainter painter(this);
    if (split < 0) {
        const QString shown = fontMetrics().elidedText(full, Qt::ElideRight, area.width());
        style()->drawItemText(&painter, area, static_cast<int>(alignment()), palette(), isEnabled(),
                              shown, foregroundRole());
        return;
    }

    // Main line (elided) above a smaller detail line.
    const QString main = fontMetrics().elidedText(full.left(split), Qt::ElideRight, area.width());
    QFont detailFont = font();
    detailFont.setPixelSize(qMax(12, fontInfo().pixelSize() - 3));
    detailFont.setBold(false);
    const QFontMetrics detailMetrics(detailFont);
    const QString detail = detailMetrics.elidedText(full.mid(split + m_detailSeparator.size()),
                                                    Qt::ElideRight, area.width());

    const int mainHeight = fontMetrics().height();
    const int top = area.top() + qMax(0, (area.height() - mainHeight - detailMetrics.height()) / 2);
    style()->drawItemText(&painter, QRect(area.left(), top, area.width(), mainHeight),
                          horizontal | Qt::AlignVCenter, palette(), isEnabled(), main, foregroundRole());

    QPalette detailPalette = palette();
    detailPalette.setColor(foregroundRole(), QColor(QString::fromLatin1(Theme::TEXT_MUTED)));
    painter.setFont(detailFont);
    style()->drawItemText(&painter, QRect(area.left(), top + mainHeight, area.width(), detailMetrics.height()),
                          horizontal | Qt::AlignVCenter, detailPalette, isEnabled(), detail, foregroundRole());
}

} // namespace ui
} // namespace multipack
