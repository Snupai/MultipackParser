from PySide6.QtWidgets import QLabel, QDialog
from PySide6.QtCore import QTimer, QRect, Qt
from PySide6.QtGui import QFont
from typing import Optional
from utils.message import MessageType

class BlinkingLabel(QLabel):
    def __init__(self, text: str, color: str, geometry: QRect, parent=None, 
                 second_color: Optional[str] = None, 
                 font: Optional[QFont] = None,
                 alignment: Optional[Qt.AlignmentFlag] = None) -> None:
        super().__init__(text, parent)
        self.setGeometry(geometry)
        self.setStyleSheet(f"color: {color};")
        self.base_color = color
        self.second_color: str | None = second_color if second_color is not None else None  # Use base color if no second color
        self.is_blinking = False
        self.timer = None
        
        if font:
            self.setFont(font)
        if alignment:
            self.setAlignment(alignment)

    def start_blinking(self) -> None:
        """Start the blinking animation"""
        if self.timer is None:
            self.timer = QTimer(self)
            if not self.second_color:
                self.timer.timeout.connect(self.toggle_visibility)
            else:
                self.timer.timeout.connect(lambda: self.change_color([self.base_color, self.second_color]))
            
        self.timer.start(500)  # Blink every 500 ms
        self.is_blinking = True
        self.show()

    def stop_blinking(self) -> None:
        """Stop the blinking animation"""
        if self.timer:
            self.timer.stop()
        self.is_blinking = False
        self.setStyleSheet(f"color: {self.base_color};")
        self.show()

    def toggle_visibility(self) -> None:
        """Instead of hiding, just make text transparent"""
        current_style = self.styleSheet()
        if "rgba" in current_style and "0)" in current_style:  # If transparent
            self.setStyleSheet(f"color: {self.base_color};")
        else:
            self.setStyleSheet("color: rgba(0,0,0,0);")  # Transparent
    
    def change_color(self, colours) -> None:
        # get current colour and set to the opposite colour
        current_colour = self.styleSheet().split(":")[1].strip()[:-1]
        if current_colour == colours[0]:
            next_colour = colours[1]
        else:
            next_colour = colours[0]
        self.setStyleSheet(f"color: {next_colour};")

    def update_text(self, text: str) -> None:
        """Update the label text"""
        self.setText(text)

    def update_color(self, color: str, second_color: Optional[str] = None) -> None:
        """Update the label colors"""
        self.base_color = color
        self.second_color = second_color
        self.setStyleSheet(f"color: {color};")
        
    def mousePressEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            from ui_files.message_dialog import MessageDialog
            from utils import global_vars
            
            if global_vars.message_manager:
                dialog = MessageDialog(global_vars.message_manager.get_all_messages(), self.parent())
                if dialog.exec() == QDialog.DialogCode.Accepted:
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
        
