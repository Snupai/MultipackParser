/**
 * @file NotificationPopup.h
 * @brief Popup notification widget for displaying alerts
 */
#ifndef MULTIPACK_UI_NOTIFICATIONPOPUP_H
#define MULTIPACK_UI_NOTIFICATIONPOPUP_H

#include <QWidget>
#include <QLabel>
#include <QTimer>

namespace multipack {
namespace ui {

/**
 * @class NotificationPopup
 * @brief A popup widget that displays alerts at the bottom-right of the screen
 *
 * Features:
 * - Semi-transparent colored background (red for errors, etc.)
 * - Blinking effect for attention
 * - Auto-close after timeout
 * - Stays on top of other windows
 */
class NotificationPopup : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Notification type determines color scheme
     */
    enum class Type {
        Info,     // Blue background
        Warning,  // Orange background
        Error,    // Red background
        Success   // Green background
    };

    /**
     * @brief Construct notification popup
     * @param parent Parent widget
     */
    explicit NotificationPopup(QWidget* parent = nullptr);

    ~NotificationPopup() override;

    /**
     * @brief Show a message with default settings
     * @param message Text to display
     * @param durationMs Duration in milliseconds (0 = indefinite)
     */
    void showMessage(const QString& message, int durationMs = 3000);

    /**
     * @brief Show a message with specific type
     * @param message Text to display
     * @param type Notification type for color scheme
     * @param durationMs Duration in milliseconds (0 = indefinite)
     */
    void showMessage(const QString& message, Type type, int durationMs = 3000);

    /**
     * @brief Enable or disable blinking effect
     * @param enabled True to enable blinking
     */
    void setBlinking(bool enabled);

    /**
     * @brief Check if popup is currently visible
     */
    bool isActive() const { return isVisible(); }

public slots:
    /**
     * @brief Close and hide the popup
     */
    void closePopup();

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void toggleBlink();

private:
    void setupUi();
    void reposition();
    void applyStyle(Type type, bool highlight = false);
    QColor colorForType(Type type, bool highlight = false) const;

    QLabel* m_label = nullptr;
    QTimer* m_closeTimer = nullptr;
    QTimer* m_blinkTimer = nullptr;

    Type m_currentType = Type::Error;
    bool m_blinking = false;
    bool m_blinkState = true;
    QString m_message;
};

} // namespace ui
} // namespace multipack

#endif // MULTIPACK_UI_NOTIFICATIONPOPUP_H
