# Startup dialogs for palette configuration

from PySide6.QtWidgets import QDialog, QVBoxLayout, QHBoxLayout, QLabel, QPushButton, QRadioButton, QSpinBox, QGroupBox, QMessageBox
from PySide6.QtCore import Qt
from . import global_vars
import logging

logger = logging.getLogger(__name__)

class PaletteConfigDialog(QDialog):
    """Dialog to configure palettes on startup."""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Palette Configuration")
        self.setMinimumWidth(600)  # Increased width
        self.setMinimumHeight(500)  # Added minimum height
        self.setWindowFlags(self.windowFlags() & ~Qt.WindowContextHelpButtonHint)
        
        # Main layout
        layout = QVBoxLayout(self)
        layout.setSpacing(20)  # Increased spacing between elements
        layout.setContentsMargins(20, 20, 20, 20)  # Increased margins
        
        # Info label
        info_label = QLabel("Please confirm the status of each palette:")
        info_label.setStyleSheet("font-weight: bold; font-size: 18px;")  # Increased font size
        layout.addWidget(info_label)
        
        # Palette 1 group
        palette1_group = QGroupBox("Palette 1")
        palette1_group.setStyleSheet("QGroupBox { font-size: 16px; font-weight: bold; } QRadioButton { font-size: 16px; min-height: 30px; }")
        palette1_layout = QVBoxLayout()
        palette1_layout.setSpacing(15)  # Increased spacing
        
        self.palette1_empty_radio = QRadioButton("Empty")
        self.palette1_not_empty_radio = QRadioButton("Not Empty")
        self.palette1_empty_radio.setChecked(True)  # Default to empty
        
        palette1_layout.addWidget(self.palette1_empty_radio)
        palette1_layout.addWidget(self.palette1_not_empty_radio)
        palette1_group.setLayout(palette1_layout)
        layout.addWidget(palette1_group)
        
        # Palette 2 group
        palette2_group = QGroupBox("Palette 2")
        palette2_group.setStyleSheet("QGroupBox { font-size: 16px; font-weight: bold; } QRadioButton { font-size: 16px; min-height: 30px; }")
        palette2_layout = QVBoxLayout()
        palette2_layout.setSpacing(15)  # Increased spacing
        
        self.palette2_empty_radio = QRadioButton("Empty")
        self.palette2_not_empty_radio = QRadioButton("Not Empty")
        self.palette2_empty_radio.setChecked(True)  # Default to empty
        
        palette2_layout.addWidget(self.palette2_empty_radio)
        palette2_layout.addWidget(self.palette2_not_empty_radio)
        palette2_group.setLayout(palette2_layout)
        layout.addWidget(palette2_group)
        
        # Active palette selection
        active_palette_group = QGroupBox("Active Palette")
        active_palette_group.setStyleSheet("QGroupBox { font-size: 16px; font-weight: bold; } QRadioButton { font-size: 16px; min-height: 30px; }")
        active_palette_layout = QVBoxLayout()
        active_palette_layout.setSpacing(15)  # Increased spacing
        
        self.active_palette_none = QRadioButton("None (0)")
        self.active_palette_1 = QRadioButton("Palette 1")
        self.active_palette_2 = QRadioButton("Palette 2")
        self.active_palette_none.setChecked(True)  # Default to none
        
        active_palette_layout.addWidget(self.active_palette_none)
        active_palette_layout.addWidget(self.active_palette_1)
        active_palette_layout.addWidget(self.active_palette_2)
        active_palette_group.setLayout(active_palette_layout)
        layout.addWidget(active_palette_group)
        
        # Buttons
        button_layout = QHBoxLayout()
        self.confirm_button = QPushButton("Confirm")
        self.confirm_button.setStyleSheet("font-size: 16px; min-height: 40px; padding: 5px 15px;")  # Larger button
        self.confirm_button.clicked.connect(self.accept)
        button_layout.addStretch()
        button_layout.addWidget(self.confirm_button)
        button_layout.addStretch()
        layout.addLayout(button_layout)
        
        # Connect signals to update UI state
        self.palette1_not_empty_radio.toggled.connect(self.update_ui_state)
        self.palette2_not_empty_radio.toggled.connect(self.update_ui_state)
    
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