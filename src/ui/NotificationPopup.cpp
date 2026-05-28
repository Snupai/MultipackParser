/**
 * @file NotificationPopup.cpp
 * @brief Implementation of popup notification widget
 */
#include "multipack/ui/NotificationPopup.h"

#include <QApplication>
#include <QScreen>
#include <QHBoxLayout>
#include <QPainter>
#include <QDebug>

namespace multipack {
namespace ui {

NotificationPopup::NotificationPopup(QWidget* parent)
    : QWidget(parent)
{
    // Set window flags for popup behavior
    setWindowFlags(Qt::Tool |
                   Qt::FramelessWindowHint |
                   Qt::WindowStaysOnTopHint |
                   Qt::X11BypassWindowManagerHint);

    // Don't steal focus when shown
    setAttribute(Qt::WA_ShowWithoutActivating);

    setupUi();

    // Setup timers
    m_closeTimer = new QTimer(this);
    m_closeTimer->setSingleShot(true);
    connect(m_closeTimer, &QTimer::timeout, this, &NotificationPopup::closePopup);

    m_blinkTimer = new QTimer(this);
    connect(m_blinkTimer, &QTimer::timeout, this, &NotificationPopup::toggleBlink);

    qDebug() << "NotificationPopup - initialized";
}

NotificationPopup::~NotificationPopup()
{
    if (m_blinkTimer) {
        m_blinkTimer->stop();
    }
    if (m_closeTimer) {
        m_closeTimer->stop();
    }
}

void NotificationPopup::setupUi()
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(15, 15, 30, 15);

    m_label = new QLabel(this);
    m_label->setStyleSheet("font-size: 14px; background-color: transparent;");
    m_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_label);

    // Apply default error style
    applyStyle(Type::Error);
}

void NotificationPopup::showMessage(const QString& message, int durationMs)
{
    showMessage(message, Type::Error, durationMs);
}

void NotificationPopup::showMessage(const QString& message, Type type, int durationMs)
{
    m_message = message;
    m_currentType = type;
    m_label->setText(message);

    // Apply styling
    applyStyle(type);

    // Calculate size based on text
    QFontMetrics fm(m_label->font());
    int textWidth = fm.horizontalAdvance(message);
    int width = textWidth + 80;
    int height = 70;
    setFixedSize(width, height);

    // Position at bottom-right of screen
    reposition();

    // Start close timer if duration > 0
    if (durationMs > 0) {
        m_closeTimer->start(durationMs);
    } else {
        m_closeTimer->stop();
    }

    // Start blinking
    setBlinking(true);

    // Show the popup
    show();
    raise();

    qDebug() << "NotificationPopup - showing message:" << message;
}

void NotificationPopup::setBlinking(bool enabled)
{
    m_blinking = enabled;
    if (enabled) {
        m_blinkState = true;
        m_blinkTimer->start(500);  // Blink every 500ms
    } else {
        m_blinkTimer->stop();
        applyStyle(m_currentType, false);
    }
}

void NotificationPopup::closePopup()
{
    setBlinking(false);
    hide();
    qDebug() << "NotificationPopup - closed";
}

void NotificationPopup::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    raise();
    activateWindow();
}

void NotificationPopup::closeEvent(QCloseEvent* event)
{
    setBlinking(false);
    QWidget::closeEvent(event);
}

void NotificationPopup::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw rounded rectangle background
    QColor bgColor = colorForType(m_currentType, !m_blinkState);
    painter.setBrush(bgColor);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), 10, 10);

    // Draw right-pointing arrow indicator
    int arrowSize = 15;
    int arrowX = width() - arrowSize - 5;
    int arrowY = height() / 2;

    QPolygon arrow;
    arrow << QPoint(arrowX, arrowY - arrowSize / 2)
          << QPoint(arrowX + arrowSize, arrowY)
          << QPoint(arrowX, arrowY + arrowSize / 2);

    painter.setBrush(Qt::white);
    painter.drawPolygon(arrow);
}

void NotificationPopup::toggleBlink()
{
    m_blinkState = !m_blinkState;
    update();  // Trigger repaint
}

void NotificationPopup::reposition()
{
    if (QScreen* screen = QApplication::primaryScreen()) {
        QRect screenGeometry = screen->availableGeometry();

        int x = screenGeometry.right() - width() - 20;
        int y = screenGeometry.bottom() - height() - 20;

        move(x, y);
    }
}

void NotificationPopup::applyStyle(Type type, bool highlight)
{
    m_currentType = type;
    QString colorStr;
    QColor color = colorForType(type, highlight);

    QString styleSheet = QString(R"(
        QWidget {
            background-color: rgba(%1, %2, %3, 220);
            border-radius: 10px;
            color: white;
            font-weight: bold;
            padding: 12px;
        }
    )").arg(color.red()).arg(color.green()).arg(color.blue());

    setStyleSheet(styleSheet);
}

QColor NotificationPopup::colorForType(Type type, bool highlight) const
{
    switch (type) {
        case Type::Info:
            return highlight ? QColor(100, 150, 255) : QColor(50, 100, 200);
        case Type::Warning:
            return highlight ? QColor(255, 180, 100) : QColor(200, 130, 50);
        case Type::Success:
            return highlight ? QColor(100, 200, 100) : QColor(50, 150, 50);
        case Type::Error:
        default:
            return highlight ? QColor(255, 100, 100) : QColor(200, 50, 50);
    }
}

} // namespace ui
} // namespace multipack
