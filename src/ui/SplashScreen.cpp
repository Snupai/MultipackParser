/**
 * @file SplashScreen.cpp
 * @brief Implementation of application splash screen
 */

#include "multipack/ui/SplashScreen.h"
#include <QApplication>
#include <QPainter>
#include <QFontMetrics>
#include <QStyleOptionProgressBar>
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout>

namespace multipack {
namespace ui {

// =========================================================================
// SplashScreen Implementation
// =========================================================================

SplashScreen::SplashScreen(QWidget* parent)
    : QSplashScreen(QPixmap())
{
    Q_UNUSED(parent);
    // Load the logo
    m_logoPixmap = QPixmap(":/Szaidel Logo/imgs/logoszaidel-transparent-big.png");
    if (m_logoPixmap.isNull()) {
        qWarning() << "Failed to load splash screen logo, using fallback";
        // Create a simple text-based logo if image fails to load
        m_logoPixmap = QPixmap(400, 200);
        m_logoPixmap.fill(Qt::white);
        QPainter painter(&m_logoPixmap);
        painter.setPen(Qt::black);
        painter.setFont(QFont("Arial", 24, QFont::Bold));
        painter.drawText(m_logoPixmap.rect(), Qt::AlignCenter, "MultipackParser");
        painter.end();
    }
    
    createBackground();
    setupUi();
}

SplashScreen::~SplashScreen() = default;

void SplashScreen::setProgress(int value)
{
    m_currentProgress = qBound(0, value, 100);
    if (m_progressBar) {
        m_progressBar->setValue(m_currentProgress);
    }
    repaintContents();
}

int SplashScreen::getProgress() const
{
    return m_currentProgress;
}

void SplashScreen::setStatusText(const QString& text)
{
    m_currentText = text;
    if (m_statusLabel) {
        m_statusLabel->setText(text);
    }
    repaintContents();
}

QString SplashScreen::getStatusText() const
{
    return m_currentText;
}

void SplashScreen::updateProgress(int progress, const QString& text)
{
    setProgress(progress);
    setStatusText(text);
}

void SplashScreen::showInstant()
{
    QSplashScreen::show();
    QApplication::processEvents();
}

void SplashScreen::repaintContents()
{
    QSplashScreen::repaint();
    QApplication::processEvents();
}

void SplashScreen::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.drawPixmap(0, 0, m_backgroundPixmap);
    
    // Draw progress indicator manually if we don't have embedded widgets
    if (!m_progressBar || !m_statusLabel) {
        drawProgressManually(&painter);
    }
}

void SplashScreen::setupUi()
{
    // Create progress bar
    m_progressBar = std::make_unique<QProgressBar>(this);
    setupProgressBar();
    
    // Create status label
    m_statusLabel = std::make_unique<QLabel>(this);
    setupStatusLabel();
    
    updateComponentPositions();
}

void SplashScreen::createBackground()
{
    // Create white background
    m_backgroundPixmap = QPixmap(m_logoPixmap.size());
    m_backgroundPixmap.fill(Qt::white);
    
    // Paint logo onto background
    QPainter painter(&m_backgroundPixmap);
    painter.drawPixmap(0, 0, m_logoPixmap);
    painter.end();
    
    // Set the pixmap for QSplashScreen
    setPixmap(m_backgroundPixmap);
}

void SplashScreen::setupProgressBar()
{
    if (!m_progressBar) return;
    
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(m_currentProgress);
    m_progressBar->setTextVisible(true);
    m_progressBar->setAlignment(Qt::AlignCenter);
    
    // Apply styling that matches Python version
    m_progressBar->setStyleSheet(R"(
        QProgressBar {
            border: 2px solid grey;
            border-radius: 5px;
            text-align: center;
            background-color: #f0f0f0;
        }
        QProgressBar::chunk {
            background-color: rgb(54, 71, 228);
            width: 10px;
            margin: 0.5px;
        }
    )");
}

void SplashScreen::setupStatusLabel()
{
    if (!m_statusLabel) return;
    
    m_statusLabel->setText(m_currentText);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("color: #333333; font-size: 14px;");
}

void SplashScreen::updateComponentPositions()
{
    if (!m_progressBar || !m_statusLabel) return;
    
    int splashWidth = m_backgroundPixmap.width();
    int splashHeight = m_backgroundPixmap.height();
    
    // Position progress bar at bottom
    m_progressBar->setGeometry(
        splashWidth / 4,
        splashHeight - 50,
        splashWidth / 2,
        20
    );
    
    // Position status label above progress bar
    m_statusLabel->setGeometry(
        splashWidth / 4,
        splashHeight - 80,
        splashWidth / 2,
        30
    );
}

void SplashScreen::drawProgressManually(QPainter* painter)
{
    if (!painter) return;
    
    int splashWidth = m_backgroundPixmap.width();
    int splashHeight = m_backgroundPixmap.height();
    
    // Draw progress bar background
    QRect progressRect(
        splashWidth / 4,
        splashHeight - 50,
        splashWidth / 2,
        20
    );
    
    painter->setBrush(QBrush(QColor(240, 240, 240)));
    painter->setPen(QPen(Qt::gray, 2));
    painter->drawRoundedRect(progressRect, 5, 5);
    
    // Draw progress bar fill
    if (m_currentProgress > 0) {
        int fillWidth = (progressRect.width() * m_currentProgress) / 100;
        QRect fillRect(
            progressRect.left(),
            progressRect.top(),
            fillWidth,
            progressRect.height()
        );
        
        painter->setBrush(QBrush(QColor(54, 71, 228)));
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(fillRect, 3, 3);
    }
    
    // Draw progress percentage
    painter->setPen(QPen(Qt::black));
    QFont font = painter->font();
    font.setBold(true);
    painter->setFont(font);
    
    QString progressText = QString("%1%").arg(m_currentProgress);
    painter->drawText(progressRect, Qt::AlignCenter, progressText);
    
    // Draw status text
    QRect textRect(
        splashWidth / 4,
        splashHeight - 80,
        splashWidth / 2,
        30
    );
    
    painter->setPen(QPen(QColor(51, 51, 51)));
    font.setBold(false);
    font.setPointSize(10);
    painter->setFont(font);
    
    QFontMetrics metrics(font);
    QString elidedText = metrics.elidedText(m_currentText, Qt::ElideRight, textRect.width());
    painter->drawText(textRect, Qt::AlignCenter, elidedText);
}

// =========================================================================
// InstantSplashScreen Implementation
// =========================================================================

InstantSplashScreen::InstantSplashScreen(QWidget* parent)
    : QSplashScreen(QPixmap())
{
    Q_UNUSED(parent);
    createInstantBackground();
}

InstantSplashScreen::~InstantSplashScreen() = default;

void InstantSplashScreen::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.drawPixmap(0, 0, m_backgroundPixmap);
    
    // Draw "Loading..." text
    QRect textRect = m_backgroundPixmap.rect();
    textRect.adjust(0, textRect.height() - 50, 0, 0);
    
    painter.setPen(QPen(Qt::black));
    QFont font = painter.font();
    font.setPointSize(14);
    painter.setFont(font);
    
    painter.drawText(textRect, Qt::AlignCenter, "Loading...");
}

void InstantSplashScreen::createInstantBackground()
{
    // Load the logo
    m_logoPixmap = QPixmap(":/Szaidel Logo/imgs/logoszaidel-transparent-big.png");
    if (m_logoPixmap.isNull()) {
        // Fallback
        m_logoPixmap = QPixmap(400, 200);
        m_logoPixmap.fill(Qt::white);
        QPainter painter(&m_logoPixmap);
        painter.setPen(Qt::black);
        painter.setFont(QFont("Arial", 24, QFont::Bold));
        painter.drawText(m_logoPixmap.rect(), Qt::AlignCenter, "MultipackParser");
        painter.end();
    }
    
    // Create white background
    m_backgroundPixmap = QPixmap(m_logoPixmap.size());
    m_backgroundPixmap.fill(Qt::white);
    
    // Paint logo onto background
    QPainter painter(&m_backgroundPixmap);
    painter.drawPixmap(0, 0, m_logoPixmap);
    painter.end();
    
    setPixmap(m_backgroundPixmap);
}

} // namespace ui
} // namespace multipack
