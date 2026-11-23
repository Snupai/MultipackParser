/**
 * @file PaletteConfigDialog.cpp
 * @brief Implementation of UR20 palette configuration dialog
 */

#include "multipack/ui/PaletteConfigDialog.h"
#include "multipack/core/GlobalState.h"
#include "multipack/ui/Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QApplication>
#include <QPainter>

namespace multipack {
namespace ui {

PaletteConfigDialog::PaletteConfigDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Palettenkonfiguration"));
    setMinimumWidth(1000);
    setMaximumHeight(680);  // Fits the 1280x720 panel
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    
    loadImages();
    setupUi();
    
    // Initialize to current GlobalState values
    updateUiState();
}

PaletteConfigDialog::~PaletteConfigDialog() = default;

QJsonObject PaletteConfigDialog::getConfiguration() const
{
    QJsonObject config;
    
    config["palette1_empty"] = m_palette1EmptyRadio->isChecked();
    config["palette2_empty"] = m_palette2EmptyRadio->isChecked();
    
    if (m_activePalette1Radio->isChecked()) {
        config["active_palette"] = 1;
    } else if (m_activePalette2Radio->isChecked()) {
        config["active_palette"] = 2;
    } else {
        config["active_palette"] = 0;
    }
    
    return config;
}

void PaletteConfigDialog::updateActivePalettePreview()
{
    QPixmap pixmap;
    
    if (m_activePalette1Radio->isChecked()) {
        pixmap = m_palette1Pixmap;
    } else if (m_activePalette2Radio->isChecked()) {
        pixmap = m_palette2Pixmap;
    } else {
        pixmap = m_noPalettePixmap;
    }
    
    if (!pixmap.isNull()) {
        QPixmap scaledPixmap = pixmap.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_activePaletteImage->setPixmap(scaledPixmap);
        m_activePaletteImage->setText("");
    } else {
        m_activePaletteImage->clear();
        m_activePaletteImage->setText(QStringLiteral("Keine Palette"));
    }
}

void PaletteConfigDialog::updateUiState()
{
    // Disable selecting a non-empty palette as active
    m_activePalette1Radio->setEnabled(m_palette1EmptyRadio->isChecked());
    m_activePalette2Radio->setEnabled(m_palette2EmptyRadio->isChecked());
    
    // If current selection is invalid, reset to none
    if (m_activePalette1Radio->isChecked() && !m_activePalette1Radio->isEnabled()) {
        m_activePaletteNoneRadio->setChecked(true);
    }
    if (m_activePalette2Radio->isChecked() && !m_activePalette2Radio->isEnabled()) {
        m_activePaletteNoneRadio->setChecked(true);
    }
}

void PaletteConfigDialog::setupUi()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 20, 24, 20);

    m_infoLabel = new QLabel(QStringLiteral("Bitte den Zustand der beiden Palettenplätze bestätigen:"));
    Theme::setRole(m_infoLabel, "heading");
    layout->addWidget(m_infoLabel);

    // Landscape panel: the three choices sit side by side.
    QHBoxLayout* columns = new QHBoxLayout();
    columns->setSpacing(16);
    setupPaletteGroup(columns, QStringLiteral("Palette 1"), QStringLiteral("Palettenplatz links"),
                      m_palette1Pixmap, m_palette1Image, m_palette1EmptyRadio, m_palette1NotEmptyRadio);
    setupPaletteGroup(columns, QStringLiteral("Palette 2"), QStringLiteral("Palettenplatz rechts"),
                      m_palette2Pixmap, m_palette2Image, m_palette2EmptyRadio, m_palette2NotEmptyRadio);
    setupActivePaletteGroup(columns);
    layout->addLayout(columns);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_confirmButton = new QPushButton(QStringLiteral("Bestätigen"));
    m_confirmButton->setMinimumWidth(280);
    Theme::setScale(m_confirmButton, "medium");
    Theme::setRole(m_confirmButton, "primary");
    connect(m_confirmButton, &QPushButton::clicked, this, &QDialog::accept);

    buttonLayout->addStretch();
    buttonLayout->addWidget(m_confirmButton);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);

    // Connect signals to update UI state
    connect(m_palette1NotEmptyRadio, &QRadioButton::toggled, this, &PaletteConfigDialog::updateUiState);
    connect(m_palette2NotEmptyRadio, &QRadioButton::toggled, this, &PaletteConfigDialog::updateUiState);

    // Connect active palette selection signals
    connect(m_activePaletteNoneRadio, &QRadioButton::toggled, this, &PaletteConfigDialog::updateActivePalettePreview);
    connect(m_activePalette1Radio, &QRadioButton::toggled, this, &PaletteConfigDialog::updateActivePalettePreview);
    connect(m_activePalette2Radio, &QRadioButton::toggled, this, &PaletteConfigDialog::updateActivePalettePreview);

    // Initialize preview
    updateActivePalettePreview();
}

namespace {

QLabel* makePreviewImage()
{
    auto* image = new QLabel();
    image->setFixedSize(140, 140);
    image->setStyleSheet(QStringLiteral(
        "border: 1px solid #d3d9e2; border-radius: 6px; padding: 5px; background-color: white;"));
    image->setAlignment(Qt::AlignCenter);
    return image;
}

} // namespace

void PaletteConfigDialog::setupPaletteGroup(QHBoxLayout* layout, const QString& title,
                                            const QString& description, const QPixmap& pixmap,
                                            QLabel*& image, QRadioButton*& emptyRadio,
                                            QRadioButton*& notEmptyRadio)
{
    QGroupBox* group = new QGroupBox(title);
    QVBoxLayout* groupLayout = new QVBoxLayout(group);
    groupLayout->setSpacing(8);

    image = makePreviewImage();
    if (!pixmap.isNull()) {
        image->setPixmap(pixmap.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    groupLayout->addWidget(image, 0, Qt::AlignHCenter);

    QLabel* descriptionLabel = new QLabel(description);
    descriptionLabel->setAlignment(Qt::AlignHCenter);
    Theme::setRole(descriptionLabel, "muted");
    groupLayout->addWidget(descriptionLabel);

    emptyRadio = new QRadioButton(QStringLiteral("Leer"));
    notEmptyRadio = new QRadioButton(QStringLiteral("Belegt"));
    emptyRadio->setChecked(true);
    groupLayout->addWidget(emptyRadio);
    groupLayout->addWidget(notEmptyRadio);
    groupLayout->addStretch();

    layout->addWidget(group, 1);
}

void PaletteConfigDialog::setupActivePaletteGroup(QHBoxLayout* layout)
{
    QGroupBox* group = new QGroupBox(QStringLiteral("Aktive Palette"));
    QVBoxLayout* groupLayout = new QVBoxLayout(group);
    groupLayout->setSpacing(8);

    m_activePaletteImage = makePreviewImage();
    groupLayout->addWidget(m_activePaletteImage, 0, Qt::AlignHCenter);

    m_activePaletteNoneRadio = new QRadioButton(QStringLiteral("Keine (0)"));
    m_activePalette1Radio = new QRadioButton(QStringLiteral("Palette 1"));
    m_activePalette2Radio = new QRadioButton(QStringLiteral("Palette 2"));
    m_activePaletteNoneRadio->setChecked(true);
    groupLayout->addWidget(m_activePaletteNoneRadio);
    groupLayout->addWidget(m_activePalette1Radio);
    groupLayout->addWidget(m_activePalette2Radio);
    groupLayout->addStretch();

    layout->addWidget(group, 1);
}

void PaletteConfigDialog::loadImages()
{
    // Load palette images - using fallback if resources not found
    m_palette1Pixmap = QPixmap(":/icons/UR20/scanner3nio.png");
    m_palette2Pixmap = QPixmap(":/icons/UR20/scanner1nio.png");
    
    // Create no palette pixmap
    m_noPalettePixmap = createNoPalettePixmap();
}

QPixmap PaletteConfigDialog::createNoPalettePixmap() const
{
    // Create a simple "No Palette" pixmap
    QPixmap pixmap(120, 120);
    pixmap.fill(Qt::white);
    
    QPainter painter(&pixmap);
    painter.setPen(Qt::gray);
    painter.setFont(QFont("Arial", 12));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, QStringLiteral("Keine\nPalette"));
    painter.end();
    
    return pixmap;
}

} // namespace ui
} // namespace multipack