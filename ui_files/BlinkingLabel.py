from PySide6.QtWidgets import QLabel, QDialog
from PySide6.QtCore import QTimer, QRect, Qt
from PySide6.QtGui import QFont
from typing import Optional
from utils.message import MessageType
from utils import global_vars
logger = global_vars.logger

class BlinkingLabel(QLabel):
    """BlinkingLabel class.

    Args:
        QLabel (QLabel): The parent class of the BlinkingLabel
    """
    def __init__(self, text: str, color: str, geometry: QRect, parent=None, 
                 second_color: Optional[str] = None, 
                 font: Optional[QFont] = None,
                 alignment: Optional[Qt.AlignmentFlag] = None) -> None:
        """Initialize the BlinkingLabel.

        Args:
            text (str): The text to display
            color (str): The color of the text
            geometry (QRect): The geometry of the label
            parent (QWidget, optional): The parent of the label. Defaults to None.
            second_color (Optional[str], optional): The second color of the text. Defaults to None.
            font (Optional[QFont], optional): The font of the text. Defaults to None.
            alignment (Optional[Qt.AlignmentFlag], optional): The alignment of the text. Defaults to None.
        """
        logger.debug(f"Creating BlinkingLabel: {text} (color: {color})")
        super().__init__(text, parent)
        self.setGeometry(geometry)
        self.setStyleSheet(f"color: {color};")
        self.blink: bool = False
        self.base_color: str = color
        self.second_color: Optional[str] = second_color if second_color is not None else None  # Use base color if no second color
        self.timer = None
        
        if font:
            self.setFont(font)
        if alignment:
            self.setAlignment(alignment)

    def start_blinking(self) -> None:
        """Start the blinking animation.
        """
        logger.debug("Starting blink animation")
        if self.timer is None:
            self.timer = QTimer(self)
            if not self.second_color:
                self.timer.timeout.connect(self.toggle_visibility)
            else:
                self.timer.timeout.connect(lambda: self.change_color([self.base_color, self.second_color]))
            
        self.timer.start(500)  # Blink every 500 ms
        self.blink = True
        self.show()

    def stop_blinking(self) -> None:
        """Stop the blinking animation.
        """
        logger.debug("Stopping blink animation")
        if self.timer:
            self.timer.stop()
        self.blink = False
        self.setStyleSheet(f"color: {self.base_color};")
        self.show()

    def toggle_visibility(self) -> None:
        """Toggle the visibility of the label.
        """
        current_style = self.styleSheet()
        if "rgba" in current_style and "0)" in current_style:  # If transparent
            self.setStyleSheet(f"color: {self.base_color};")
        else:
            self.setStyleSheet("color: rgba(0,0,0,0);")  # Transparent
    
    def change_color(self, colours) -> None:
        """Change the color of the label.

        Args:
            colours (list): The list of colours to change to
        """
        # get current colour and set to the opposite colour
        current_colour = self.styleSheet().split(":")[1].strip()[:-1]
        if current_colour == colours[0]:
            next_colour = colours[1]
        else:
            next_colour = colours[0]
        self.setStyleSheet(f"color: {next_colour};")

    def update_text(self, text: str) -> None:
        """Update the text of the label.

        Args:
            text (str): The text to update the label with
        """
        logger.debug(f"Updating label text: {text}")
        self.setText(text)

    def update_color(self, color: str, second_color: Optional[str] = None) -> None:
        """Update the color of the label.

        Args:
            color (str): The color of the label
            second_color (Optional[str], optional): The second color of the label. Defaults to None.
        """
        logger.debug(f"Updating label colors - primary: {color}, secondary: {second_color}")
        self.base_color = color
        self.second_color = second_color
        self.setStyleSheet(f"color: {color};")
        
    def mousePressEvent(self, event) -> None:
        """Handle the mouse press event."""
        if event.button() == Qt.MouseButton.LeftButton:
            logger.debug("BlinkingLabel clicked")
            from ui_files.message_dialog import MessageDialog
            
            if global_vars.message_manager:
                logger.debug("Creating MessageDialog")
                messages = global_vars.message_manager.get_all_messages()
                logger.debug(f"Number of messages: {len(messages)}")
                
                dialog = MessageDialog(messages, self.window())
                dialog.setAttribute(Qt.WidgetAttribute.WA_DeleteOnClose, False)  # Prevent auto-deletion
                dialog.setModal(True)  # Ensure dialog is modal
                
                logger.debug("Showing MessageDialog")
                result = dialog.exec()
                logger.debug(f"Dialog result: {result}")
                
                if result == QDialog.DialogCode.Accepted:
                    # Acknowledge selected messages
                    messages = global_vars.message_manager.get_all_messages()
                    for row in dialog.selected_for_acknowledgment:
                        global_vars.message_manager.acknowledge_message(messages[row])
                    
                    # Get latest unacknowledged message
                    active_messages = global_vars.message_manager.get_active_messages()
                    if active_messages:
                        latest = active_messages[-1]
                        self.update_text(latest.text)
                        color = {
                            MessageType.INFO: "black",
                            MessageType.WARNING: "orange",
                            MessageType.ERROR: "red"
                        }.get(latest.type, "black")
                        self.update_color(color)
                    else:
                        # Show default operational message
                        self.update_text("Everything operational")
                        self.update_color("green")
                        self.stop_blinking()
            else:
                logger.warning("No message manager available")
        
