from PySide6.QtWidgets import (QDialog, QVBoxLayout, QTableWidget, 
                              QTableWidgetItem, QPushButton, QHeaderView,
                              QHBoxLayout, QCheckBox)
from PySide6.QtCore import Qt
from PySide6.QtGui import QColor
from utils.message import Message, MessageType

class MessageDialog(QDialog):
    def __init__(self, messages, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Meldungsliste")
        self.setMinimumSize(800, 400)
        
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
        self.ack_button = QPushButton("Quittieren")
        self.ack_button.clicked.connect(self.acknowledge_selected)
        button_layout.addWidget(self.show_history)
        button_layout.addStretch()
        button_layout.addWidget(self.ack_button)
        
        layout.addWidget(self.table)
        layout.addLayout(button_layout)
        
        # Add messages to table
        self.update_messages(messages)
        
    def update_messages(self, messages):
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
                self.table.item(i, j).setBackground(color)
                
    def acknowledge_selected(self):
        selected_rows = set(item.row() for item in self.table.selectedItems())
        self.selected_for_acknowledgment = selected_rows
        self.accept()