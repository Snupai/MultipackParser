#ifndef MULTIPACK_UI_SPLASHSCREEN_H
#define MULTIPACK_UI_SPLASHSCREEN_H

#include <QSplashScreen>
#include <QProgressBar>
#include <QLabel>
#include <QPixmap>
#include <QPainter>
#include <memory>

namespace multipack {
namespace ui {

/**
 * @brief Application splash screen with progress indicator
 * 
 * This class provides a splash screen that shows during application startup
 * with a progress bar and status text to provide feedback to users
 * during the initialization process.
 */
class SplashScreen : public QSplashScreen
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent widget (typically nullptr for splash screen)
     */
    explicit SplashScreen(QWidget* parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~SplashScreen() override;

    /**
     * @brief Set progress value (0-100)
     * @param value Progress percentage
     */
    void setProgress(int value);
    
    /**
     * @brief Get current progress value
     * @return Current progress percentage
     */
    int getProgress() const;
    
    /**
     * @brief Set status text message
     * @param text Status message to display
     */
    void setStatusText(const QString& text);
    
    /**
     * @brief Get current status text
     * @return Current status message
     */
    QString getStatusText() const;
    
    /**
     * @brief Update both progress and status
     * @param progress Progress percentage (0-100)
     * @param text Status message to display
     */
    void updateProgress(int progress, const QString& text);
    
    /**
     * @brief Show the splash screen immediately
     */
    void showInstant();
    
    /**
     * @brief Repaint the splash screen with current progress
     */
    void repaintContents();

protected:
    /**
     * @brief Override paint event for custom drawing
     * @param event Paint event
     */
    void paintEvent(QPaintEvent* event) override;

private:
    // UI components
    std::unique_ptr<QProgressBar> m_progressBar;
    std::unique_ptr<QLabel> m_statusLabel;
    
    // Progress tracking
    int m_currentProgress = 0;
    QString m_currentText;
    
    // Visual elements
    QPixmap m_logoPixmap;
    QPixmap m_backgroundPixmap;
    
    /**
     * @brief Initialize UI components
     */
    void setupUi();
    
    /**
     * @brief Create the splash screen background
     */
    void createBackground();
    
    /**
     * @brief Setup progress bar styling
     */
    void setupProgressBar();
    
    /**
     * @brief Setup status label styling
     */
    void setupStatusLabel();
    
    /**
     * @brief Update component positions based on splash size
     */
    void updateComponentPositions();

    /**
     * @brief Draw progress when widgets are unavailable
     * @param painter Painter to draw with
     */
    void drawProgressManually(QPainter* painter);
};

/**
 * @brief Instant splash screen shown before full initialization
 * 
 * This is a minimal splash screen that can be shown immediately
 * while the full splash screen is being prepared.
 */
class InstantSplashScreen : public QSplashScreen
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent widget
     */
    explicit InstantSplashScreen(QWidget* parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~InstantSplashScreen() override;

protected:
    /**
     * @brief Override paint event
     * @param event Paint event
     */
    void paintEvent(QPaintEvent* event) override;

private:
    QPixmap m_logoPixmap;
    QPixmap m_backgroundPixmap;
    
    /**
     * @brief Create instant splash background
     */
    void createInstantBackground();
};

} // namespace ui
} // namespace multipack

#endif // MULTIPACK_UI_SPLASHSCREEN_H
