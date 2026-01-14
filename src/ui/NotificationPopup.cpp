/**
 * @file NotificationPopup.cpp
 */
#include "multipack/ui/NotificationPopup.h"
#include <QDebug>
namespace multipack { namespace ui {
NotificationPopup::NotificationPopup(QWidget* parent) : QWidget(parent) {}
void NotificationPopup::showMessage(const QString& msg, int duration) {
    Q_UNUSED(msg); Q_UNUSED(duration);
    qDebug() << "NotificationPopup::showMessage - TODO";
}
}} // namespace
