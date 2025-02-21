from PySide6.QtWidgets import (QDialog, QVBoxLayout, QTableWidget, 
                              QTableWidgetItem, QPushButton, QHeaderView,
                              QHBoxLayout, QCheckBox)
from PySide6.QtCore import Qt
from PySide6.QtGui import QColor, QFont
from utils.message import Message, MessageType
from utils import global_vars
import logging

logger = logging.getLogger(__name__)

class MessageDialog(QDialog):
    """Dialog for displaying and acknowledging messages.

    Args:
        QDialog (QDialog): The parent class of the message dialog.
    """
    def __init__(self, messages, parent=None):
        """Initialize the message dialog.

        Args:
            messages (list): The list of messages to be displayed.
            parent (QWidget, optional): The parent widget. Defaults to None.
        """
        logger.debug(f"Initializing MessageDialog with {len(messages)} messages")
        super().__init__(parent)
        self.setWindowTitle("Meldungsliste")
        self.setMinimumSize(800, 400)
        
        # Remove standard window frame but keep dialog behavior
        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint | 
            Qt.WindowType.WindowStaysOnTopHint |
            Qt.WindowType.Dialog
        )
        
        # Ensure dialog is modal
        self.setModal(True)
        
        layout = QVBoxLayout(self)
        
        # Add checkbox for showing history
        self.show_history = QCheckBox("Verlauf anzeigen")
        self.show_history.setChecked(False)
        self.show_history.stateChanged.connect(lambda: self.update_messages(messages))
        
        # Create table
        self.table = QTableWidget()
        self.table.setColumnCount(4)
        self.table.setHorizontalHeaderLabels(["Zeitstempel", "Typ", "Meldung", "Status"])
        self.table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeMode.Stretch)
        
        # Button layout
        button_layout = QHBoxLayout()
        
        # Create custom close button with larger size
        self.close_button = QPushButton("Schließen")
        self.close_button.setMinimumSize(120, 40)  # Make button bigger
        font = QFont()
        font.setPointSize(12)  # Increase font size
        self.close_button.setFont(font)
        self.close_button.clicked.connect(self.handle_close)
        
        self.ack_button = QPushButton("Quittieren")
        self.ack_button.setMinimumSize(120, 40)  # Make button bigger
        self.ack_button.setFont(font)  # Use same font
        self.ack_button.clicked.connect(self.acknowledge_selected)
        
        button_layout.addWidget(self.show_history)
        button_layout.addStretch()
        button_layout.addWidget(self.ack_button)
        button_layout.addWidget(self.close_button)
        
        layout.addWidget(self.table)
        layout.addLayout(button_layout)
        
        # Add messages to table
        self.update_messages(messages)
        
    def update_messages(self, messages):
        """Update the messages in the table.

        Args:
            messages (list): The list of messages to be displayed.
        """
        # Filter messages based on checkbox state
        filtered_messages = messages if self.show_history.isChecked() else [m for m in messages if not m.acknowledged]
        
        self.table.setRowCount(len(filtered_messages))
        for i, msg in enumerate(filtered_messages):
            self.table.setItem(i, 0, QTableWidgetItem(msg.timestamp.strftime('%Y-%m-%d %H:%M:%S')))
            self.table.setItem(i, 1, QTableWidgetItem(msg.type.name))
            self.table.setItem(i, 2, QTableWidgetItem(msg.text))
            self.table.setItem(i, 3, QTableWidgetItem("Quittiert" if msg.acknowledged else "Aktiv"))
            
            # Set color based on message type
            color = {
                MessageType.INFO: QColor("#ffffff"),
                MessageType.WARNING: QColor("#fff3cd"),
                MessageType.ERROR: QColor("#f8d7da")
            }.get(msg.type, QColor("#ffffff"))
            
            # Make acknowledged messages slightly transparent
            if msg.acknowledged:
                color.setAlpha(128)
            
            for j in range(4):
                item = self.table.item(i, j)
                item.setBackground(color)
                # Disable selection for acknowledged or blocked messages
                if (msg.acknowledged or 
                    (global_vars.message_manager and msg.text in global_vars.message_manager._blocked_messages)):
                    item.setFlags(item.flags() & ~Qt.ItemFlag.ItemIsSelectable)

        # Enable/disable acknowledge button based on selection
        self.ack_button.setEnabled(len(self.table.selectedItems()) > 0)
        
    def acknowledge_selected(self):
        """Acknowledge the selected messages.
        """
        selected_rows = set(item.row() for item in self.table.selectedItems())
        self.selected_for_acknowledgment = selected_rows
        self.accept()

    def handle_close(self) -> None:
        """Handle the close button click."""
        self.reject()

    def mousePressEvent(self, event) -> None:
        """Handle mouse press events for dragging the window."""
        if event.button() == Qt.MouseButton.LeftButton:
            self._drag_pos = event.pos()

    def mouseMoveEvent(self, event) -> None:
        """Handle mouse move events for dragging the window."""
        if hasattr(self, '_drag_pos'):
            self.move(self.pos() + event.pos() - self._drag_pos)