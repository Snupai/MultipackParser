/**
 * @file ScaledImageLabel.h
 * @brief Image label that scales its pixmap to the space it gets, keeping the aspect ratio
 */
#ifndef MULTIPACK_UI_SCALEDIMAGELABEL_H
#define MULTIPACK_UI_SCALEDIMAGELABEL_H

#include <QLabel>

namespace multipack {
namespace ui {

/**
 * @class ScaledImageLabel
 * @brief Shows pixmap() as large as the label allows, never distorted
 *
 * QLabel::scaledContents stretches to the label rectangle; the scanner image
 * must stay readable when the page has less room (UR20 notice bar).
 */
class ScaledImageLabel : public QLabel
{
    Q_OBJECT

public:
    explicit ScaledImageLabel(QWidget* parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
};

} // namespace ui
} // namespace multipack

#endif // MULTIPACK_UI_SCALEDIMAGELABEL_H
