/**
 * @file PasswordDialog.h
 * @brief Password entry dialog
 */
#ifndef MULTIPACK_UI_PASSWORDDIALOG_H
#define MULTIPACK_UI_PASSWORDDIALOG_H

#include <QDialog>

namespace multipack { namespace ui {

class PasswordDialog : public QDialog {
    Q_OBJECT
public:
    explicit PasswordDialog(QWidget* parent = nullptr);
    QString password() const;
};

}} // namespace multipack::ui
#endif
