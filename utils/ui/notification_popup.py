"""
Popup notification system for displaying alerts at the bottom right of the screen.
"""

import logging
from PySide6.QtWidgets import QLabel, QWidget, QHBoxLayout, QApplication
from PySide6.QtCore import Qt, QTimer, QPoint, QSize
from PySide6.QtGui import QPainter, QColor, QPen, QPolygon

from utils.system.core import global_vars

logger = logging.getLogger(__name__)

class ArrowPopup(QWidget):
    """
    A popup widget that displays at the bottom right of the screen with a right-pointing arrow.
    """
    
    def __init__(self, parent=None, message="", timeout=0):
        """
        Initialize the popup with a message and optional timeout.
        
        Args:
            parent: Parent widget
            message: Message to display in the popup
            timeout: Time in milliseconds before the popup disappears (0 = no timeout)
        """
        # Set global_vars.main_window as parent if no parent is provided
        if parent is None and global_vars.main_window is not None:
            parent = global_vars.main_window
        
        super().__init__(parent)
        
        # Set window flags to ensure it stays on top
        self.setWindowFlags(
            Qt.WindowType.Tool |
            Qt.WindowType.FramelessWindowHint |
            Qt.WindowType.WindowStaysOnTopHint |
            Qt.WindowType.X11BypassWindowManagerHint
        )
        
        # Set attribute to ensure it stays on top
        self.setAttribute(Qt.WidgetAttribute.WA_ShowWithoutActivating)
        
        # Set background color to a semi-transparent red to indicate an error
        self.setStyleSheet("""
            QWidget {
                background-color: rgba(200, 50, 50, 220);
                border-radius: 10px;
                color: white;
                font-weight: bold;
                padding: 12px;
            }
        """)
        
        self.message = message
        self.arrow_size = 20
        self.is_blinking = False
        self.blink_state = True
        
        # Create layout
        self.layout = QHBoxLayout(self)
        self.layout.setContentsMargins(15, 15, 30, 15)
        
        # Create message label
        self.label = QLabel(message)
        self.label.setStyleSheet("font-size: 14px; background-color: transparent;")
        self.label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.layout.addWidget(self.label)
        
        # Set timer if timeout is not 0
        if timeout > 0:
            QTimer.singleShot(timeout, self.close)
        
        # Position the widget at the bottom right of the screen
        self.reposition()
        
        # Create blink timer
        self.blink_timer = QTimer(self)
        self.blink_timer.timeout.connect(self.blink)
        self.start_blinking()
    
    def start_blinking(self):
        """Start the blinking effect."""
        if not self.is_blinking:
            self.is_blinking = True
            self.blink_timer.start(500)  # Blink every 500ms
    
    def stop_blinking(self):
        """Stop the blinking effect."""
        if self.is_blinking:
            self.is_blinking = False
            self.blink_timer.stop()
            self.setStyleSheet("""
                QWidget {
                    background-color: rgba(200, 50, 50, 220);
                    border-radius: 10px;
                    color: white;
                    font-weight: bold;
                    padding: 12px;
                }
            """)
    
    def blink(self):
        """Toggle the blink state."""
        self.blink_state = not self.blink_state
        if self.blink_state:
            self.setStyleSheet("""
                QWidget {
                    background-color: rgba(200, 50, 50, 220);
                    border-radius: 10px;
                    color: white;
                    font-weight: bold;
                    padding: 12px;
                }
            """)
        else:
            self.setStyleSheet("""
                QWidget {
                    background-color: rgba(255, 100, 100, 220);
                    border-radius: 10px;
                    color: white;
                    font-weight: bold;
                    padding: 12px;
                }
            """)
        # Make sure it stays on top during blinking
        self.raise_()
    
    def showEvent(self, event):
        """Custom show event to ensure the widget is raised to the top."""
        super().showEvent(event)
        # Make sure it's at the front
        self.raise_()
        self.activateWindow()
    
    def closeEvent(self, event):
        """Custom close event to ensure the blink timer is stopped."""
        self.stop_blinking()
        super().closeEvent(event)
    
    def reposition(self):
        """Position the popup at the bottom right of the screen."""
        # Get screen geometry
        screen = QApplication.primaryScreen().geometry()
        
        # Calculate popup size based on text
        text_width = self.label.fontMetrics().boundingRect(self.message).width()
        size = QSize(text_width + 80, 70)  # Add padding
        
        # Calculate position (bottom right with margin)
        margin = 20
        pos = QPoint(
            screen.width() - size.width() - margin,
            screen.height() - size.height() - margin
        )
        
        self.resize(size)
        self.move(pos)
    
    def paintEvent(self, event):
        """Custom paint event to draw the arrow."""
        super().paintEvent(event)
        
        # Draw a right-pointing arrow
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        
        # Set arrow color to white
        painter.setPen(QPen(QColor(255, 255, 255), 2))
        painter.setBrush(QColor(255, 255, 255))
        
        # Create arrow polygon pointing to the right
        arrow = QPolygon([
            QPoint(self.width() - 20, self.height() // 2 - 10),  # Top
            QPoint(self.width() - 5, self.height() // 2),       # Tip
            QPoint(self.width() - 20, self.height() // 2 + 10)   # Bottom
        ])
        
        painter.drawPolygon(arrow)
        
        # Draw outline of the widget with rounded corners
        painter.setPen(QPen(QColor(180, 30, 30), 2))
        painter.setBrush(Qt.BrushStyle.NoBrush)
        painter.drawRoundedRect(self.rect().adjusted(1, 1, -1, -1), 10, 10)

# Global popup instance
zwischenlage_popup = None

def check_zwischenlage_status():
    """
    Check the zwischenlage status and show/hide popup accordingly.
    """
    global zwischenlage_popup
    
    if global_vars.UR20_zwischenlage is True:
        if zwischenlage_popup is None or not zwischenlage_popup.isVisible():
            logger.debug("Showing zwischenlage popup")
            # Create popup with main window as parent
            zwischenlage_popup = ArrowPopup(
                parent=global_vars.main_window,
                message="Zwischenlage legen und mit Reset bestätigen."
            )
            zwischenlage_popup.show()
            # Ensure it's shown on top
            zwischenlage_popup.raise_()
            zwischenlage_popup.activateWindow()
        else:
            # Ensure existing popup is visible and on top
            zwischenlage_popup.raise_()
            zwischenlage_popup.activateWindow()
    elif zwischenlage_popup is not None and zwischenlage_popup.isVisible():
        logger.debug("Hiding zwischenlage popup")
        zwischenlage_popup.close()
        zwischenlage_popup = None 