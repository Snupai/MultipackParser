/**
 * @file NotificationPopup.h
 */
#ifndef MULTIPACK_UI_NOTIFICATIONPOPUP_H
#define MULTIPACK_UI_NOTIFICATIONPOPUP_H
#include <QWidget>
namespace multipack { namespace ui {
class NotificationPopup : public QWidget {
    Q_OBJECT
public:
    explicit NotificationPopup(QWidget* parent = nullptr);
    void showMessage(const QString& message, int durationMs = 3000);
};
}} // namespace
#endif
