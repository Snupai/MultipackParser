/**
 * @file PasswordDialog.cpp
 * @brief Implementation of password entry dialog
 */
#include "multipack/ui/PasswordDialog.h"
#include "multipack/config/SettingsManager.h"
#include "multipack/config/ConfigDefaults.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QCryptographicHash>
#include <QApplication>
#include <QScreen>
#include <QDebug>

namespace multipack {
namespace ui {

PasswordDialog::PasswordDialog(QWidget* parent, config::SettingsManager* settings)
    : QDialog(parent)
    , m_settings(settings)
{
    setupUi();

    // Position dialog near top of screen, centered horizontally
    if (QScreen* screen = QApplication::primaryScreen()) {
        QRect geometry = screen->availableGeometry();
        int x = geometry.x() + (geometry.width() - width()) / 2;
        int y = geometry.y() + 20;
        move(x, y);
    }

    // Set window flags for modal behavior
    setWindowFlags(windowFlags() |
                   Qt::WindowStaysOnTopHint |
                   Qt::FramelessWindowHint);
    setWindowModality(Qt::WindowModal);

    // Enable input method for virtual keyboard
    setAttribute(Qt::WA_InputMethodEnabled, true);
    m_lineEdit->setAttribute(Qt::WA_InputMethodEnabled, true);

    // Focus the password input
    m_lineEdit->setFocus();

    qDebug() << "PasswordDialog - initialized";
}

void PasswordDialog::setupUi()
{
    setWindowTitle("Passwort");
    setFixedSize(385, 136);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(10);

    // Error label (hidden by default)
    m_errorLabel = new QLabel(this);
    m_errorLabel->setStyleSheet("color: red; font-weight: bold;");
    m_errorLabel->hide();
    mainLayout->addWidget(m_errorLabel);

    // Password input row
    QHBoxLayout* inputLayout = new QHBoxLayout();

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setEchoMode(QLineEdit::Password);
    m_lineEdit->setMaxLength(20);
    m_lineEdit->setPlaceholderText("Passwort");
    m_lineEdit->setInputMethodHints(Qt::ImhHiddenText |
                                     Qt::ImhNoAutoUppercase |
                                     Qt::ImhNoPredictiveText |
                                     Qt::ImhPreferNumbers |
                                     Qt::ImhSensitiveData);
    m_lineEdit->setFocusPolicy(Qt::StrongFocus);

    // Connect return key to accept
    connect(m_lineEdit, &QLineEdit::returnPressed, this, &PasswordDialog::accept);

    inputLayout->addWidget(m_lineEdit);

    // Button box
    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal,
        this);
    inputLayout->addWidget(m_buttonBox);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &PasswordDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &PasswordDialog::reject);

    mainLayout->addLayout(inputLayout);

    // Styling
    setStyleSheet(R"(
        QDialog {
            background-color: #f5f5f5;
            border: 1px solid #c0c0c0;
            border-radius: 4px;
        }
        QLineEdit {
            padding: 8px;
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            background-color: white;
            font-size: 14px;
        }
        QLineEdit:focus {
            border-color: #0078d4;
        }
        QPushButton {
            padding: 6px 16px;
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            background-color: #e0e0e0;
        }
        QPushButton:hover {
            background-color: #d0d0d0;
        }
        QPushButton:pressed {
            background-color: #c0c0c0;
        }
    )");
}

QString PasswordDialog::password() const
{
    return m_lineEdit ? m_lineEdit->text() : QString();
}

void PasswordDialog::setSettingsManager(config::SettingsManager* settings)
{
    m_settings = settings;
}

void PasswordDialog::accept()
{
    QString enteredPassword = m_lineEdit->text();

    if (enteredPassword.isEmpty()) {
        m_errorLabel->setText("Bitte Passwort eingeben");
        m_errorLabel->show();
        return;
    }

    // Verify against stored password or master password
    if (verifyPassword(enteredPassword) || verifyMasterPassword(enteredPassword)) {
        qDebug() << "PasswordDialog - password accepted";
        m_accepted = true;
        m_lineEdit->clear();
        QDialog::accept();
    } else {
        qDebug() << "PasswordDialog - password rejected";
        m_errorLabel->setText("Falsches Passwort");
        m_errorLabel->show();
        m_lineEdit->clear();
        m_lineEdit->setFocus();
    }
}

void PasswordDialog::reject()
{
    qDebug() << "PasswordDialog - cancelled";
    m_accepted = false;
    m_lineEdit->clear();
    QDialog::reject();
}

bool PasswordDialog::verifyPassword(const QString& password)
{
    if (!m_settings) {
        qDebug() << "PasswordDialog - no settings manager, refusing verification";
        return false;
    }

    if (!m_settings->hasAdminPassword()) {
        qDebug() << "PasswordDialog - no admin password set";
        return false;
    }

    return m_settings->verifyAdminPassword(password);
}

bool PasswordDialog::verifyMasterPassword(const QString& password)
{
    if (!m_settings) {
        return false;
    }

    // Get the stored password hash to extract the salt
    QString storedHash = m_settings->value(config::Keys::ADMIN_PASSWORD_HASH).toString();
    if (storedHash.isEmpty()) {
        return false;
    }

    // Parse salt from stored hash (format: "salt$hash")
    int separatorIndex = storedHash.indexOf('$');
    if (separatorIndex < 0) {
        return false;
    }

    QString saltHex = storedHash.left(separatorIndex);
    QByteArray salt = QByteArray::fromHex(saltHex.toLatin1());

    // Hash the master password with the same salt
    QString masterHash = hashWithSalt(QString(MASTER_PASSWORD), salt);

    // Hash entered password with same salt
    QString enteredHash = hashWithSalt(password, salt);

    return enteredHash == masterHash;
}

QString PasswordDialog::hashWithSalt(const QString& password, const QByteArray& salt) const
{
    QByteArray saltedPassword = salt + password.toUtf8();
    QByteArray hash = QCryptographicHash::hash(saltedPassword, QCryptographicHash::Sha256);
    return QString(hash.toHex());
}

} // namespace ui
} // namespace multipack
