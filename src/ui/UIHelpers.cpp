/**
 * @file UIHelpers.cpp
 */
#include "multipack/ui/UIHelpers.h"
#include <QWidget>
#include <QScreen>
#include <QGuiApplication>
namespace multipack { namespace ui { namespace UIHelpers {
void centerOnScreen(QWidget* widget) {
    if (auto* screen = QGuiApplication::primaryScreen()) {
        auto geo = screen->availableGeometry();
        widget->move((geo.width() - widget->width()) / 2,
                     (geo.height() - widget->height()) / 2);
    }
}
void setStyleSheet(QWidget* widget, const QString& path) {
    Q_UNUSED(widget); Q_UNUSED(path);
}
QString formatDimensions(double l, double w, double h) {
    return QString("%1 x %2 x %3 mm").arg(l).arg(w).arg(h);
}
}}} // namespace
