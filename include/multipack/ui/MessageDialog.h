/**
 * @file MessageDialog.h
 */
#ifndef MULTIPACK_UI_MESSAGEDIALOG_H
#define MULTIPACK_UI_MESSAGEDIALOG_H
#include <QDialog>
namespace multipack { namespace ui {
class MessageDialog : public QDialog {
    Q_OBJECT
public:
    explicit MessageDialog(QWidget* parent = nullptr);
    static void showInfo(const QString& title, const QString& message, QWidget* parent = nullptr);
    static void showWarning(const QString& title, const QString& message, QWidget* parent = nullptr);
    static void showError(const QString& title, const QString& message, QWidget* parent = nullptr);
};
}} // namespace
#endif
