/**
 * @file PaletteConfigDialog.cpp
 * @brief Implementation of UR20 palette configuration dialog
 */

#include "multipack/ui/PaletteConfigDialog.h"
#include "multipack/core/GlobalState.h"
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
    setWindowTitle("Palette Configuration");
    setMinimumWidth(600);  // Reduced width
    setMaximumHeight(680);  // Set maximum height for RPi display
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
        m_activePaletteImage->setText("No Palette Selected");
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
    layout->setSpacing(10);  // Reduced spacing between elements
    layout->setContentsMargins(15, 15, 15, 15);  // Reduced margins
    
    // Info label
    m_infoLabel = new QLabel("Please confirm the status of each palette:");
    m_infoLabel->setStyleSheet("font-weight: bold; font-size: 16px;");  // Slightly reduced font size
    layout->addWidget(m_infoLabel);
    
    // Setup palette groups
    setupPalette1Group(layout);
    setupPalette2Group(layout);
    setupActivePaletteGroup(layout);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_confirmButton = new QPushButton("Confirm");
    m_confirmButton->setStyleSheet("font-size: 14px; min-height: 35px; padding: 5px 15px;");
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

void PaletteConfigDialog::setupPalette1Group(QVBoxLayout* layout)
{
    QHBoxLayout* palette1Container = new QHBoxLayout();
    palette1Container->setSpacing(10);  // Reduced spacing
    
    // Image for Palette 1
    m_palette1Image = new QLabel();
    m_palette1Image->setFixedSize(140, 140);  // Reduced image size
    m_palette1Image->setStyleSheet("border: 2px solid #cccccc; border-radius: 5px; padding: 5px; background-color: white;");
    
    if (!m_palette1Pixmap.isNull()) {
        QPixmap scaledPixmap1 = m_palette1Pixmap.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_palette1Image->setPixmap(scaledPixmap1);
    }
    m_palette1Image->setAlignment(Qt::AlignCenter);
    palette1Container->addWidget(m_palette1Image);
    
    // Palette 1 controls
    QGroupBox* palette1Group = new QGroupBox("Palette 1");
    styleGroupBoxes(palette1Group);
    
    QVBoxLayout* palette1Layout = new QVBoxLayout();
    palette1Layout->setSpacing(8);  // Reduced spacing
    
    QLabel* palette1Desc = new QLabel("Left side palette position");
    palette1Desc->setStyleSheet("font-size: 12px; color: #666666;");  // Reduced font size
    palette1Layout->addWidget(palette1Desc);
    
    m_palette1EmptyRadio = new QRadioButton("Empty");
    m_palette1NotEmptyRadio = new QRadioButton("Not Empty");
    m_palette1EmptyRadio->setChecked(true);
    
    styleRadioButtons();
    
    palette1Layout->addWidget(m_palette1EmptyRadio);
    palette1Layout->addWidget(m_palette1NotEmptyRadio);
    palette1Group->setLayout(palette1Layout);
    palette1Container->addWidget(palette1Group);
    layout->addLayout(palette1Container);
}

void PaletteConfigDialog::setupPalette2Group(QVBoxLayout* layout)
{
    QHBoxLayout* palette2Container = new QHBoxLayout();
    palette2Container->setSpacing(10);  // Reduced spacing
    
    // Image for Palette 2
    m_palette2Image = new QLabel();
    m_palette2Image->setFixedSize(140, 140);  // Reduced image size
    m_palette2Image->setStyleSheet("border: 2px solid #cccccc; border-radius: 5px; padding: 5px; background-color: white;");
    
    if (!m_palette2Pixmap.isNull()) {
        QPixmap scaledPixmap2 = m_palette2Pixmap.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_palette2Image->setPixmap(scaledPixmap2);
    }
    m_palette2Image->setAlignment(Qt::AlignCenter);
    palette2Container->addWidget(m_palette2Image);
    
    // Palette 2 controls
    QGroupBox* palette2Group = new QGroupBox("Palette 2");
    styleGroupBoxes(palette2Group);
    
    QVBoxLayout* palette2Layout = new QVBoxLayout();
    palette2Layout->setSpacing(8);  // Reduced spacing
    
    QLabel* palette2Desc = new QLabel("Right side palette position");
    palette2Desc->setStyleSheet("font-size: 12px; color: #666666;");  // Reduced font size
    palette2Layout->addWidget(palette2Desc);
    
    m_palette2EmptyRadio = new QRadioButton("Empty");
    m_palette2NotEmptyRadio = new QRadioButton("Not Empty");
    m_palette2EmptyRadio->setChecked(true);
    
    styleRadioButtons();
    
    palette2Layout->addWidget(m_palette2EmptyRadio);
    palette2Layout->addWidget(m_palette2NotEmptyRadio);
    palette2Group->setLayout(palette2Layout);
    palette2Container->addWidget(palette2Group);
    layout->addLayout(palette2Container);
}

void PaletteConfigDialog::setupActivePaletteGroup(QVBoxLayout* layout)
{
    QHBoxLayout* activePaletteContainer = new QHBoxLayout();
    
    // Active palette controls
    QGroupBox* activePaletteGroup = new QGroupBox("Active Palette");
    styleGroupBoxes(activePaletteGroup);
    activePaletteGroup->setStyleSheet("QGroupBox { font-size: 14px; font-weight: bold; } QRadioButton { font-size: 14px; min-height: 25px; }");  // Reduced sizes
    
    QVBoxLayout* activePaletteLayout = new QVBoxLayout();
    activePaletteLayout->setSpacing(8);
    
    m_activePaletteNoneRadio = new QRadioButton("None (0)");
    m_activePalette1Radio = new QRadioButton("Palette 1");
    m_activePalette2Radio = new QRadioButton("Palette 2");
    m_activePaletteNoneRadio->setChecked(true);
    
    styleRadioButtons();
    
    activePaletteLayout->addWidget(m_activePaletteNoneRadio);
    activePaletteLayout->addWidget(m_activePalette1Radio);
    activePaletteLayout->addWidget(m_activePalette2Radio);
    activePaletteGroup->setLayout(activePaletteLayout);
    activePaletteContainer->addWidget(activePaletteGroup);
    
    // Active palette preview
    QGroupBox* previewGroup = new QGroupBox("Active Palette Preview");
    previewGroup->setStyleSheet("QGroupBox { font-size: 14px; font-weight: bold; }");
    
    QVBoxLayout* previewLayout = new QVBoxLayout();
    
    m_activePaletteImage = new QLabel();
    m_activePaletteImage->setFixedSize(140, 140);
    m_activePaletteImage->setStyleSheet("border: 2px solid #cccccc; border-radius: 5px; padding: 5px; background-color: white;");
    m_activePaletteImage->setAlignment(Qt::AlignCenter);
    previewLayout->addWidget(m_activePaletteImage);
    
    previewGroup->setLayout(previewLayout);
    activePaletteContainer->addWidget(previewGroup);
    
    layout->addLayout(activePaletteContainer);
}

void PaletteConfigDialog::loadImages()
{
    // Load palette images - using fallback if resources not found
    m_palette1Pixmap = QPixmap(":/ScannerUR20/imgs/UR20/scanner3nio.png");
    m_palette2Pixmap = QPixmap(":/ScannerUR20/imgs/UR20/scanner1nio.png");
    
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
    painter.drawText(pixmap.rect(), Qt::AlignCenter, "No\nPalette");
    painter.end();
    
    return pixmap;
}

void PaletteConfigDialog::styleRadioButtons()
{
    QString style = "QRadioButton { font-size: 14px; min-height: 25px; }";
    
    if (m_palette1EmptyRadio) m_palette1EmptyRadio->setStyleSheet(style);
    if (m_palette1NotEmptyRadio) m_palette1NotEmptyRadio->setStyleSheet(style);
    if (m_palette2EmptyRadio) m_palette2EmptyRadio->setStyleSheet(style);
    if (m_palette2NotEmptyRadio) m_palette2NotEmptyRadio->setStyleSheet(style);
    
    if (m_activePaletteNoneRadio) m_activePaletteNoneRadio->setStyleSheet(style);
    if (m_activePalette1Radio) m_activePalette1Radio->setStyleSheet(style);
    if (m_activePalette2Radio) m_activePalette2Radio->setStyleSheet(style);
}

void PaletteConfigDialog::styleGroupBoxes(QGroupBox* group)
{
    if (group) {
        group->setStyleSheet("QGroupBox { font-size: 14px; font-weight: bold; } QRadioButton { font-size: 14px; min-height: 25px; }");
    }
}

void PaletteConfigDialog::styleLabel(QLabel* label, const QString& stylesheet)
{
    if (label) {
        if (stylesheet.isEmpty()) {
            label->setStyleSheet("font-size: 12px; color: #666666;");
        } else {
            label->setStyleSheet(stylesheet);
        }
    }
}

} // namespace ui
} // namespace multipack