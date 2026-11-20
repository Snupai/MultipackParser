/**
 * @file QrCodeWidget.h
 * @brief Draws a QR code for a text (vendored qrcodegen)
 */
#ifndef MULTIPACK_UI_QRCODEWIDGET_H
#define MULTIPACK_UI_QRCODEWIDGET_H

#include <QImage>
#include <QString>
#include <QWidget>

namespace multipack {
namespace ui {

/**
 * @brief Render a QR code as a 1-pixel-per-module image with a 4-module quiet zone
 * @return Null image when the text cannot be encoded
 */
[[nodiscard]] QImage renderQrCode(const QString& text);

class QrCodeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QrCodeWidget(QWidget* parent = nullptr);

    void setText(const QString& text);
    [[nodiscard]] QString text() const { return m_text; }
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_text;
    QImage m_image;
};

} // namespace ui
} // namespace multipack

#endif // MULTIPACK_UI_QRCODEWIDGET_H
