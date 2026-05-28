/**
 * @file NotificationPopupEnhanced.h
 * @brief Enhanced notification popup with arrow pointing and queue management
 */
#ifndef MULTIPACK_UI_POLISH_NOTIFICATIONPOPUPENHANCED_H
#define MULTIPACK_UI_POLISH_NOTIFICATIONPOPUPENHANCED_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QQueue>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>

namespace multipack {
namespace ui {

/**
 * @class NotificationPopupEnhanced
 * @brief Enhanced popup widget with arrow pointing to source widget and queue management
 *
 * Features:
 * - Arrow-shaped appearance pointing to specific UI elements
 * - Color-coded messages (info, warning, error, success)
 * - Auto-dismiss with user-configurable timeouts
 * - Queue management for multiple simultaneous notifications
 * - Smooth fade animations
 * - Position calculation to point to specific UI elements
 */
class NotificationPopupEnhanced : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)

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
     * @brief Notification data structure
     */
    struct NotificationData {
        QString message;
        Type type;
        int durationMs;
        QWidget* targetWidget;
        QPoint customPosition;
        
        NotificationData(const QString& msg, Type t, int dur, QWidget* target = nullptr, const QPoint& pos = QPoint())
            : message(msg), type(t), durationMs(dur), targetWidget(target), customPosition(pos) {}
    };

    /**
     * @brief Construct enhanced notification popup
     * @param parent Parent widget
     */
    explicit NotificationPopupEnhanced(QWidget* parent = nullptr);

    ~NotificationPopupEnhanced() override;

    /**
     * @brief Show a notification message
     * @param message Text to display
     * @param type Notification type for color scheme
     * @param durationMs Duration in milliseconds (0 = indefinite)
     * @param targetWidget Widget to point arrow towards
     */
    void showMessage(const QString& message, Type type = Type::Info, 
                    int durationMs = 3000, QWidget* targetWidget = nullptr);

    /**
     * @brief Show a notification at custom position
     * @param message Text to display
     * @param type Notification type for color scheme
     * @param durationMs Duration in milliseconds
     * @param position Position to show notification at
     */
    void showMessageAt(const QString& message, Type type, int durationMs, const QPoint& position);

    /**
     * @brief Add notification to queue (will be shown when current one finishes)
     * @param message Text to display
     * @param type Notification type for color scheme
     * @param durationMs Duration in milliseconds
     * @param targetWidget Widget to point arrow towards
     */
    void queueNotification(const QString& message, Type type = Type::Info,
                          int durationMs = 3000, QWidget* targetWidget = nullptr);

    /**
     * @brief Set default timeout for notifications
     * @param timeoutMs Default timeout in milliseconds
     */
    void setDefaultTimeout(int timeoutMs);

    /**
     * @brief Get default timeout
     */
    int defaultTimeout() const { return m_defaultTimeout; }

    /**
     * @brief Set maximum queue size
     * @param maxSize Maximum number of queued notifications
     */
    void setMaxQueueSize(int maxSize);

    /**
     * @brief Clear notification queue
     */
    void clearQueue();

    /**
     * @brief Check if popup is currently active
     */
    bool isActive() const { return isVisible(); }

    /**
     * @brief Get number of queued notifications
     */
    int queuedCount() const { return m_notificationQueue.size(); }

public slots:
    /**
     * @brief Close current notification and show next if any
     */
    void closeNotification();

    /**
     * @brief Dismiss current notification immediately
     */
    void dismiss();

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void fadeIn();
    void fadeOut();
    void processNextNotification();

private:
    void setupUi();
    void setupAnimations();
    void calculatePosition();
    void applyStyle(Type type);
    QColor colorForType(Type type) const;
    void drawArrow(QPainter& painter, const QPoint& targetPoint);
    QPoint calculateArrowTarget() const;
    void updateGeometry();

    // UI Components
    QLabel* m_label = nullptr;
    QGraphicsOpacityEffect* m_opacityEffect = nullptr;

    // Animations
    QPropertyAnimation* m_fadeAnimation = nullptr;

    // Timers
    QTimer* m_closeTimer = nullptr;
    QTimer* m_queueTimer = nullptr;

    // Queue and state
    QQueue<NotificationData> m_notificationQueue;
    NotificationData* m_currentNotification = nullptr;
    int m_defaultTimeout = 3000;
    int m_maxQueueSize = 10;

    // Appearance
    Type m_currentType = Type::Info;
    qreal m_opacity = 0.0;
    int m_arrowSize = 15;
    int m_cornerRadius = 10;
    QPoint m_arrowTarget;
    bool m_hasArrowTarget = false;
};

} // namespace ui
} // namespace multipack

#endif // MULTIPACK_UI_POLISH_NOTIFICATIONPOPUPENHANCED_H