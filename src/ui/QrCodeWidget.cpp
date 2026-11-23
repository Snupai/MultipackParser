/**
 * @file QrCodeWidget.cpp
 * @brief Draws a QR code for a text (vendored qrcodegen)
 */
#include "multipack/ui/QrCodeWidget.h"

#include "qrcodegen/qrcodegen.hpp"

#include <QPainter>

#include <algorithm>
#include <cmath>
#include <exception>

namespace multipack {
namespace ui {

QImage renderQrCode(const QString& text)
{
    if (text.isEmpty()) {
        return QImage();
    }

    try {
        const QByteArray utf8 = text.toUtf8();
        const qrcodegen::QrCode qr =
            qrcodegen::QrCode::encodeText(utf8.constData(), qrcodegen::QrCode::Ecc::MEDIUM);
        constexpr int border = 4;
        const int size = qr.getSize() + 2 * border;
        QImage image(size, size, QImage::Format_Mono);
        image.setColor(0, qRgb(255, 255, 255));
        image.setColor(1, qRgb(0, 0, 0));
        image.fill(0);
        for (int y = 0; y < qr.getSize(); ++y) {
            for (int x = 0; x < qr.getSize(); ++x) {
                if (qr.getModule(x, y)) {
                    image.setPixel(x + border, y + border, 1);
                }
            }
        }
        return image;
    } catch (const std::exception&) {
        return QImage();
    }
}

QrCodeWidget::QrCodeWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void QrCodeWidget::setText(const QString& text)
{
    if (text == m_text) {
        return;
    }
    m_text = text;
    m_image = renderQrCode(text);
    update();
}

QSize QrCodeWidget::sizeHint() const
{
    return QSize(200, 200);
}

void QrCodeWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    if (m_image.isNull()) {
        return;
    }

    // A whole number of screen pixels per module keeps every module sharp,
    // also on a scaled panel; centre in the widget on a screen pixel.
    const qreal ratio = devicePixelRatioF();
    const int side = static_cast<int>(std::min(width(), height()) * ratio);
    const int scale = std::max(1, side / m_image.width());
    const qreal drawn = scale * m_image.width() / ratio;
    const QRectF target(std::floor((width() - drawn) / 2 * ratio) / ratio,
                        std::floor((height() - drawn) / 2 * ratio) / ratio, drawn, drawn);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(target, m_image);
}

} // namespace ui
} // namespace multipack
