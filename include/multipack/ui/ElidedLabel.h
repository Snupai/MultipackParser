/**
 * @file ElidedLabel.h
 * @brief Label for a slot of limited width: shortens its text instead of growing
 */
#ifndef MULTIPACK_UI_ELIDEDLABEL_H
#define MULTIPACK_UI_ELIDEDLABEL_H

#include <QLabel>
#include <QString>

namespace multipack {
namespace ui {

/**
 * @class ElidedLabel
 * @brief Plain-text label for a slot of limited width (header bar)
 *
 * text() stays the full text; only the painted text is elided with "...".
 *
 * With a detailSeparator the part after its last occurrence moves to a second,
 * smaller line: "Geladen: <name> - 5 Lagen, 80 Pakete" keeps the counts
 * visible however long the name is.
 */
class ElidedLabel : public QLabel
{
    Q_OBJECT
    Q_PROPERTY(QString detailSeparator READ detailSeparator WRITE setDetailSeparator)

public:
    explicit ElidedLabel(QWidget* parent = nullptr);

    [[nodiscard]] QString detailSeparator() const { return m_detailSeparator; }
    void setDetailSeparator(const QString& separator);

    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_detailSeparator;
};

} // namespace ui
} // namespace multipack

#endif // MULTIPACK_UI_ELIDEDLABEL_H
