/**
 * @file BlinkingLabel.h
 * @brief Blinking status label widget
 */
#ifndef MULTIPACK_UI_BLINKINGLABEL_H
#define MULTIPACK_UI_BLINKINGLABEL_H
#include <QLabel>
namespace multipack { namespace ui {
class BlinkingLabel : public QLabel {
    Q_OBJECT
public:
    explicit BlinkingLabel(QWidget* parent = nullptr);
    void setBlinking(bool blink);
    bool isBlinking() const;
private:
    bool m_blinking = false;
};
}} // namespace
#endif
