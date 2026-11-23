/**
 * @file ElidedLabel.h
 * @brief One-line label that shortens its text with "..." instead of growing
 */
#ifndef MULTIPACK_UI_ELIDEDLABEL_H
#define MULTIPACK_UI_ELIDEDLABEL_H

#include <QLabel>

namespace multipack {
namespace ui {

/**
 * @class ElidedLabel
 * @brief Plain-text label for a slot of limited width (header bar)
 *
 * text() stays the full text; only the painted text is elided. The full text
 * is offered as tooltip while it does not fit.
 */
class ElidedLabel : public QLabel
{
    Q_OBJECT

public:
    explicit ElidedLabel(QWidget* parent = nullptr);

    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
};

} // namespace ui
} // namespace multipack

#endif // MULTIPACK_UI_ELIDEDLABEL_H
