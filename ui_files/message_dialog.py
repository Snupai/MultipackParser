from PySide6.QtWidgets import (QDialog, QVBoxLayout, QTableWidget, 
                              QTableWidgetItem, QPushButton, QHeaderView)
from PySide6.QtCore import Qt
from PySide6.QtGui import QColor
from utils.message import Message, MessageType

class MessageDialog(QDialog):
    def __init__(self, messages, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Meldungsliste")
        self.setMinimumSize(800, 400)
        
        layout = QVBoxLayout(self)
        
        # Create table
        self.table = QTableWidget()
        self.table.setColumnCount(4)
        self.table.setHorizontalHeaderLabels(["Zeitstempel", "Typ", "Meldung", "Status"])
        self.table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeMode.Stretch)
        
        # Add messages to table
        self.update_messages(messages)
        
        # Add Acknowledge button
        self.ack_button = QPushButton("Quittieren")
        self.ack_button.clicked.connect(self.acknowledge_selected)
        
        layout.addWidget(self.table)
        layout.addWidget(self.ack_button)
        
    def update_messages(self, messages):
        self.table.setRowCount(len(messages))
        for i, msg in enumerate(messages):
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
            
            for j in range(4):
                self.table.item(i, j).setBackground(color)
                
    def acknowledge_selected(self):
        selected_rows = set(item.row() for item in self.table.selectedItems())
        self.selected_for_acknowledgment = selected_rows
        self.accept()