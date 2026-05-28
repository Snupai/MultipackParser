/**
 * @file MessageDialog.cpp
 */
#include "multipack/ui/MessageDialog.h"
#include <QMessageBox>
namespace multipack { namespace ui {
MessageDialog::MessageDialog(QWidget* parent) : QDialog(parent) {}
void MessageDialog::showInfo(const QString& title, const QString& msg, QWidget* parent) {
    QMessageBox::information(parent, title, msg);
}
void MessageDialog::showWarning(const QString& title, const QString& msg, QWidget* parent) {
    QMessageBox::warning(parent, title, msg);
}
void MessageDialog::showError(const QString& title, const QString& msg, QWidget* parent) {
    QMessageBox::critical(parent, title, msg);
}
}} // namespace
