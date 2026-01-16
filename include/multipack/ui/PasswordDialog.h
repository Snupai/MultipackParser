/**
 * @file PasswordDialog.h
 * @brief Password entry dialog for admin settings access
 */
#ifndef MULTIPACK_UI_PASSWORDDIALOG_H
#define MULTIPACK_UI_PASSWORDDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

// Forward declaration
namespace multipack { namespace config { class SettingsManager; } }

namespace multipack {
namespace ui {

/**
 * @class PasswordDialog
 * @brief Modal password entry dialog for admin authentication
 *
 * Provides a simple password entry interface with validation
 * against stored hashed passwords or master password.
 */
class PasswordDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Construct password dialog
     * @param parent Parent widget (typically main window)
     * @param settings Settings manager for password verification
     */
    explicit PasswordDialog(QWidget* parent = nullptr,
                           config::SettingsManager* settings = nullptr);

    ~PasswordDialog() override = default;

    /**
     * @brief Get the entered password
     * @return The password text (cleared after dialog closes)
     */
    QString password() const;

    /**
     * @brief Check if password was accepted
     * @return true if authentication succeeded
     */
    bool wasAccepted() const { return m_accepted; }

    /**
     * @brief Set settings manager for verification
     * @param settings Settings manager pointer
     */
    void setSettingsManager(config::SettingsManager* settings);

public slots:
    void accept() override;
    void reject() override;

private:
    void setupUi();
    bool verifyPassword(const QString& password);
    bool verifyMasterPassword(const QString& password);
    QString hashWithSalt(const QString& password, const QByteArray& salt) const;

    QLineEdit* m_lineEdit = nullptr;
    QDialogButtonBox* m_buttonBox = nullptr;
    QLabel* m_errorLabel = nullptr;

    config::SettingsManager* m_settings = nullptr;
    bool m_accepted = false;

    // Master password for emergency access
    static constexpr const char* MASTER_PASSWORD = "eCaXDv6V8EUE8#d!8FTb";
};

} // namespace ui
} // namespace multipack

#endif // MULTIPACK_UI_PASSWORDDIALOG_H
