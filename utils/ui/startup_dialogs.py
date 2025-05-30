# Startup dialogs for palette configuration

from PySide6.QtWidgets import QDialog, QVBoxLayout, QHBoxLayout, QLabel, QPushButton, QRadioButton, QSpinBox, QGroupBox, QMessageBox
from PySide6.QtCore import Qt
from PySide6.QtGui import QPixmap
from utils import global_vars
import logging

logger = logging.getLogger('multipack_parser')

class PaletteConfigDialog(QDialog):
    """Dialog to configure palettes on startup."""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Palette Configuration")
        self.setMinimumWidth(600)  # Reduced width
        self.setMaximumHeight(680)  # Set maximum height for RPi display
        self.setWindowFlags(self.windowFlags() & ~Qt.WindowContextHelpButtonHint)
        
        # Store pixmaps as class variables
        self.no_palette_pixmap = QPixmap()  # Empty pixmap for no selection
        self.palette1_pixmap = QPixmap(":/ScannerUR20/imgs/UR20/scanner3nio.png")
        self.palette2_pixmap = QPixmap(":/ScannerUR20/imgs/UR20/scanner1nio.png")
        
        # Main layout
        layout = QVBoxLayout(self)
        layout.setSpacing(10)  # Reduced spacing between elements
        layout.setContentsMargins(15, 15, 15, 15)  # Reduced margins
        
        # Info label
        info_label = QLabel("Please confirm the status of each palette:")
        info_label.setStyleSheet("font-weight: bold; font-size: 16px;")  # Slightly reduced font size
        layout.addWidget(info_label)
        
        # Palette 1 group with image
        palette1_container = QHBoxLayout()
        palette1_container.setSpacing(10)  # Reduced spacing
        
        # Image for Palette 1
        palette1_image = QLabel()
        palette1_image.setFixedSize(140, 140)  # Reduced image size
        palette1_image.setStyleSheet("border: 2px solid #cccccc; border-radius: 5px; padding: 5px; background-color: white;")
        if not self.palette1_pixmap.isNull():
            scaled_pixmap1 = self.palette1_pixmap.scaled(120, 120, Qt.KeepAspectRatio, Qt.SmoothTransformation)
            palette1_image.setPixmap(scaled_pixmap1)
        palette1_image.setAlignment(Qt.AlignCenter)
        palette1_container.addWidget(palette1_image)
        
        # Palette 1 controls
        palette1_group = QGroupBox("Palette 1")
        palette1_group.setStyleSheet("QGroupBox { font-size: 14px; font-weight: bold; } QRadioButton { font-size: 14px; min-height: 25px; }")  # Reduced sizes
        palette1_layout = QVBoxLayout()
        palette1_layout.setSpacing(8)  # Reduced spacing
        
        palette1_desc = QLabel("Left side palette position")
        palette1_desc.setStyleSheet("font-size: 12px; color: #666666;")  # Reduced font size
        palette1_layout.addWidget(palette1_desc)
        
        self.palette1_empty_radio = QRadioButton("Empty")
        self.palette1_not_empty_radio = QRadioButton("Not Empty")
        self.palette1_empty_radio.setChecked(True)
        
        palette1_layout.addWidget(self.palette1_empty_radio)
        palette1_layout.addWidget(self.palette1_not_empty_radio)
        palette1_group.setLayout(palette1_layout)
        palette1_container.addWidget(palette1_group)
        layout.addLayout(palette1_container)
        
        # Palette 2 group with image
        palette2_container = QHBoxLayout()
        palette2_container.setSpacing(10)  # Reduced spacing
        
        # Image for Palette 2
        palette2_image = QLabel()
        palette2_image.setFixedSize(140, 140)  # Reduced image size
        palette2_image.setStyleSheet("border: 2px solid #cccccc; border-radius: 5px; padding: 5px; background-color: white;")
        if not self.palette2_pixmap.isNull():
            scaled_pixmap2 = self.palette2_pixmap.scaled(120, 120, Qt.KeepAspectRatio, Qt.SmoothTransformation)
            palette2_image.setPixmap(scaled_pixmap2)
        palette2_image.setAlignment(Qt.AlignCenter)
        palette2_container.addWidget(palette2_image)
        
        # Palette 2 controls
        palette2_group = QGroupBox("Palette 2")
        palette2_group.setStyleSheet("QGroupBox { font-size: 14px; font-weight: bold; } QRadioButton { font-size: 14px; min-height: 25px; }")  # Reduced sizes
        palette2_layout = QVBoxLayout()
        palette2_layout.setSpacing(8)  # Reduced spacing
        
        palette2_desc = QLabel("Right side palette position")
        palette2_desc.setStyleSheet("font-size: 12px; color: #666666;")  # Reduced font size
        palette2_layout.addWidget(palette2_desc)
        
        self.palette2_empty_radio = QRadioButton("Empty")
        self.palette2_not_empty_radio = QRadioButton("Not Empty")
        self.palette2_empty_radio.setChecked(True)
        
        palette2_layout.addWidget(self.palette2_empty_radio)
        palette2_layout.addWidget(self.palette2_not_empty_radio)
        palette2_group.setLayout(palette2_layout)
        palette2_container.addWidget(palette2_group)
        layout.addLayout(palette2_container)
        
        # Active palette selection with preview
        active_palette_container = QHBoxLayout()
        
        # Active palette controls
        active_palette_group = QGroupBox("Active Palette")
        active_palette_group.setStyleSheet("QGroupBox { font-size: 14px; font-weight: bold; } QRadioButton { font-size: 14px; min-height: 25px; }")
        active_palette_layout = QVBoxLayout()
        active_palette_layout.setSpacing(8)
        
        self.active_palette_none = QRadioButton("None (0)")
        self.active_palette_1 = QRadioButton("Palette 1")
        self.active_palette_2 = QRadioButton("Palette 2")
        self.active_palette_none.setChecked(True)
        
        active_palette_layout.addWidget(self.active_palette_none)
        active_palette_layout.addWidget(self.active_palette_1)
        active_palette_layout.addWidget(self.active_palette_2)
        active_palette_group.setLayout(active_palette_layout)
        active_palette_container.addWidget(active_palette_group)
        
        # Active palette preview
        preview_group = QGroupBox("Active Palette Preview")
        preview_group.setStyleSheet("QGroupBox { font-size: 14px; font-weight: bold; }")
        preview_layout = QVBoxLayout()
        
        self.active_palette_image = QLabel()
        self.active_palette_image.setFixedSize(140, 140)
        self.active_palette_image.setStyleSheet("border: 2px solid #cccccc; border-radius: 5px; padding: 5px; background-color: white;")
        self.active_palette_image.setAlignment(Qt.AlignCenter)
        preview_layout.addWidget(self.active_palette_image)
        
        preview_group.setLayout(preview_layout)
        active_palette_container.addWidget(preview_group)
        
        layout.addLayout(active_palette_container)
        
        # Buttons
        button_layout = QHBoxLayout()
        self.confirm_button = QPushButton("Confirm")
        self.confirm_button.setStyleSheet("font-size: 14px; min-height: 35px; padding: 5px 15px;")
        self.confirm_button.clicked.connect(self.accept)
        button_layout.addStretch()
        button_layout.addWidget(self.confirm_button)
        button_layout.addStretch()
        layout.addLayout(button_layout)
        
        # Connect signals to update UI state
        self.palette1_not_empty_radio.toggled.connect(self.update_ui_state)
        self.palette2_not_empty_radio.toggled.connect(self.update_ui_state)
        
        # Connect active palette selection signals
        self.active_palette_none.toggled.connect(self.update_active_palette_preview)
        self.active_palette_1.toggled.connect(self.update_active_palette_preview)
        self.active_palette_2.toggled.connect(self.update_active_palette_preview)
        
        # Initialize preview
        self.update_active_palette_preview()
    
    def update_active_palette_preview(self):
        """Update the active palette preview image based on selection."""
        if self.active_palette_1.isChecked():
            pixmap = self.palette1_pixmap
        elif self.active_palette_2.isChecked():
            pixmap = self.palette2_pixmap
        else:  # None selected
            pixmap = self.no_palette_pixmap
            
        if not pixmap.isNull():
            scaled_pixmap = pixmap.scaled(120, 120, Qt.KeepAspectRatio, Qt.SmoothTransformation)
            self.active_palette_image.setPixmap(scaled_pixmap)
        else:
            self.active_palette_image.clear()
            self.active_palette_image.setText("No Palette Selected")
    
    def update_ui_state(self):
        """Update UI state based on palette status selections."""
        # Disable selecting a non-empty palette as active
        self.active_palette_1.setEnabled(self.palette1_empty_radio.isChecked())
        self.active_palette_2.setEnabled(self.palette2_empty_radio.isChecked())
        
        # If current selection is invalid, reset to none
        if self.active_palette_1.isChecked() and not self.active_palette_1.isEnabled():
            self.active_palette_none.setChecked(True)
        if self.active_palette_2.isChecked() and not self.active_palette_2.isEnabled():
            self.active_palette_none.setChecked(True)
    
    def get_configuration(self):
        """Get the selected configuration."""
        palette1_empty = self.palette1_empty_radio.isChecked()
        palette2_empty = self.palette2_empty_radio.isChecked()
        
        if self.active_palette_1.isChecked():
            active_palette = 1
        elif self.active_palette_2.isChecked():
            active_palette = 2
        else:
            active_palette = 0
            
        # Log the user's choices when they click confirm
        logger.info(f"User confirmed palette configuration - Palette 1: {'Empty' if palette1_empty else 'Not Empty'}, "
                   f"Palette 2: {'Empty' if palette2_empty else 'Not Empty'}, "
                   f"Selected active palette: {active_palette}")
            
        return {
            "palette1_empty": palette1_empty,
            "palette2_empty": palette2_empty,
            "active_palette": active_palette
        }


def show_palette_config_dialog(parent=None):
    """Show the palette configuration dialog and apply settings.
    
    Args:
        parent: Parent widget
        
    Returns:
        bool: True if configuration was successful, False otherwise
    """
    dialog = PaletteConfigDialog(parent)
    
    # Make sure dialog appears in front of main window
    dialog.setWindowModality(Qt.ApplicationModal)
    
    # Show the dialog
    result = dialog.exec()
    
    if result == QDialog.Accepted:
        config = dialog.get_configuration()
        
        # Apply configuration
        global_vars.UR20_palette1_empty = config["palette1_empty"]
        global_vars.UR20_palette2_empty = config["palette2_empty"]
        global_vars.UR20_active_palette = config["active_palette"]
        
        # Log the configuration
        logger.info(f"Palette configuration set: Palette 1 empty: {global_vars.UR20_palette1_empty}, "
                   f"Palette 2 empty: {global_vars.UR20_palette2_empty}, "
                   f"Active palette: {global_vars.UR20_active_palette}")
        
        # If a non-empty palette was selected as active, show warning
        if ((global_vars.UR20_active_palette == 1 and not global_vars.UR20_palette1_empty) or
            (global_vars.UR20_active_palette == 2 and not global_vars.UR20_palette2_empty)):
            QMessageBox.warning(
                parent,
                "Invalid Configuration",
                "Cannot set a non-empty palette as active. Active palette has been reset to none (0)."
            )
            global_vars.UR20_active_palette = 0
            return False
        
        return True
    
    return False 