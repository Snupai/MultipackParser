from PySide6.QtWidgets import (QDialog, QVBoxLayout, QTableWidget, 
                              QTableWidgetItem, QPushButton, QHeaderView,
                              QHBoxLayout, QCheckBox)
from PySide6.QtCore import Qt, QRect
from PySide6.QtGui import QColor, QFont, QGuiApplication
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
        
        # Remove standard window frame and set dialog flags
        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint | 
            Qt.WindowType.WindowStaysOnTopHint |
            Qt.WindowType.Dialog
        )
        
        # Center the dialog on screen
        screen = QGuiApplication.primaryScreen().geometry()
        self.setGeometry(
            (screen.width() - 800) // 2,
            (screen.height() - 400) // 2,
            800, 400
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
        self.table.setSelectionMode(QTableWidget.SelectionMode.NoSelection)  # Disable selection
        self.table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)  # Disable editing
        
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
        self.ack_button.setMinimumSize(120, 40)
        self.ack_button.setFont(font)
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
        filtered_messages = messages if self.show_history.isChecked() else [m for m in messages if not m.acknowledged]
        
        self.table.setRowCount(len(filtered_messages))
        for i, msg in enumerate(filtered_messages):
            # Create read-only items
            timestamp_item = QTableWidgetItem(msg.timestamp.strftime('%Y-%m-%d %H:%M:%S'))
            type_item = QTableWidgetItem(msg.type.name)
            text_item = QTableWidgetItem(msg.text)
            status_item = QTableWidgetItem("Quittiert" if msg.acknowledged else "Aktiv")
            
            # Set items as not selectable and not editable
            flags = Qt.ItemFlag.ItemIsEnabled
            timestamp_item.setFlags(flags)
            type_item.setFlags(flags)
            text_item.setFlags(flags)
            status_item.setFlags(flags)
            
            self.table.setItem(i, 0, timestamp_item)
            self.table.setItem(i, 1, type_item)
            self.table.setItem(i, 2, text_item)
            self.table.setItem(i, 3, status_item)
            
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
                self.table.item(i, j).setBackground(color)

    def acknowledge_selected(self):
        """Acknowledge all non-blocked messages."""
        if not global_vars.message_manager:
            return
            
        unblocked_messages = [
            msg for msg in self.get_visible_messages() 
            if not msg.acknowledged and 
            msg.text not in global_vars.message_manager._blocked_messages
        ]
        
        # Acknowledge messages without closing dialog
        for msg in unblocked_messages:
            global_vars.message_manager.acknowledge_message(msg)
            
        # Update the display
        self.update_messages(global_vars.message_manager.get_all_messages())

    def get_visible_messages(self):
        """Get the currently visible messages based on filter."""
        messages = global_vars.message_manager.get_all_messages()
        return messages if self.show_history.isChecked() else [m for m in messages if not m.acknowledged]

    def handle_close(self) -> None:
        """Handle the close button click."""
        self.reject()