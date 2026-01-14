/**
 * @file UIHelpers.h
 */
#ifndef MULTIPACK_UI_UIHELPERS_H
#define MULTIPACK_UI_UIHELPERS_H
#include <QString>
class QWidget;
namespace multipack { namespace ui { namespace UIHelpers {
    void centerOnScreen(QWidget* widget);
    void setStyleSheet(QWidget* widget, const QString& path);
    QString formatDimensions(double l, double w, double h);
}}} // namespace
#endif
