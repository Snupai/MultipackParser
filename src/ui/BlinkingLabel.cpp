/**
 * @file BlinkingLabel.cpp
 */
#include "multipack/ui/BlinkingLabel.h"
#include <QDebug>
namespace multipack { namespace ui {
BlinkingLabel::BlinkingLabel(QWidget* parent) : QLabel(parent) {}
void BlinkingLabel::setBlinking(bool blink) { m_blinking = blink; }
bool BlinkingLabel::isBlinking() const { return m_blinking; }
}} // namespace
