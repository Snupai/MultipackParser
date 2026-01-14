/**
 * @file PasswordDialog.cpp
 * @brief Implementation of password dialog
 */
#include "multipack/ui/PasswordDialog.h"
#include <QDebug>

namespace multipack { namespace ui {

PasswordDialog::PasswordDialog(QWidget* parent) : QDialog(parent) {
    qDebug() << "PasswordDialog::PasswordDialog - TODO";
}

QString PasswordDialog::password() const { return QString(); }

}} // namespace multipack::ui
