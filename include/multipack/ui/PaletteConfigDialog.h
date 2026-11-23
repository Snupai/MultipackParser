#ifndef MULTIPACK_UI_PALETTECONFIGDIALOG_H
#define MULTIPACK_UI_PALETTECONFIGDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>
#include <QJsonObject>

namespace multipack {
namespace ui {

/**
 * @brief UR20 palette configuration dialog
 * 
 * This dialog provides configuration for UR20 robot's dual palette system,
 * allowing users to set palette status (empty/not empty) and select the active palette.
 * Features image previews and validation logic.
 */
class PaletteConfigDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent widget
     */
    explicit PaletteConfigDialog(QWidget* parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~PaletteConfigDialog() override;

    /**
     * @brief Get the current configuration
     * @return Configuration as JSON object
     */
    QJsonObject getConfiguration() const;

private slots:
    /**
     * @brief Update active palette preview image based on selection
     */
    void updateActivePalettePreview();
    
    /**
     * @brief Update UI state based on palette status selections
     */
    void updateUiState();

private:
    // UI Components
    QLabel* m_infoLabel;
    QLabel* m_palette1Image;
    QLabel* m_palette2Image;
    QLabel* m_activePaletteImage;
    
    QRadioButton* m_palette1EmptyRadio;
    QRadioButton* m_palette1NotEmptyRadio;
    QRadioButton* m_palette2EmptyRadio;
    QRadioButton* m_palette2NotEmptyRadio;
    
    QRadioButton* m_activePaletteNoneRadio;
    QRadioButton* m_activePalette1Radio;
    QRadioButton* m_activePalette2Radio;
    
    QPushButton* m_confirmButton;
    
    // Pixmaps
    QPixmap m_noPalettePixmap;
    QPixmap m_palette1Pixmap;
    QPixmap m_palette2Pixmap;
    
    /**
     * @brief Setup the dialog layout
     */
    void setupUi();
    
    /**
     * @brief One column "Palette N": position image, description, empty/occupied choice
     */
    void setupPaletteGroup(QHBoxLayout* layout, const QString& title, const QString& description,
                           const QPixmap& pixmap, QLabel*& image,
                           QRadioButton*& emptyRadio, QRadioButton*& notEmptyRadio);

    /**
     * @brief Setup active palette selection group
     */
    void setupActivePaletteGroup(QHBoxLayout* layout);

    /**
     * @brief Load images for palette previews
     */
    void loadImages();
    
    /**
     * @brief Create preview pixmap for "no palette" state
     */
    QPixmap createNoPalettePixmap() const;
};

} // namespace ui
} // namespace multipack

#endif // MULTIPACK_UI_PALETTECONFIGDIALOG_H