from PySide6.QtWidgets import QLabel
from PySide6.QtCore import QTimer, QRect, Qt
from PySide6.QtGui import QFont

class BlinkingLabel(QLabel):
    def __init__(self, text, color:str, geometry:QRect, parent=None, second_color:str=None, font:QFont=None, alignment:Qt.AlignmentFlag=None):
        super().__init__(text, parent)
        self.setGeometry(geometry)
        self.setStyleSheet(f"color: {color};")
        self.base_color = color
        self.second_color = second_color
        self.is_blinking = False
        self.timer = None
        
        if font:
            self.setFont(font)
        if alignment:
            self.setAlignment(alignment)

    def start_blinking(self):
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

    def stop_blinking(self):
        """Stop the blinking animation"""
        if self.timer:
            self.timer.stop()
        self.is_blinking = False
        self.setStyleSheet(f"color: {self.base_color};")
        self.show()

    def toggle_visibility(self):
        self.setVisible(not self.isVisible())
    
    def change_color(self, colours):
        # get current colour and set to the opposite colour
        current_colour = self.styleSheet().split(":")[1].strip()[:-1]
        if current_colour == colours[0]:
            next_colour = colours[1]
        else:
            next_colour = colours[0]
        self.setStyleSheet(f"color: {next_colour};")

    def update_text(self, text):
        """Update the label text"""
        self.setText(text)

    def update_color(self, color, second_color=None):
        """Update the label colors"""
        self.base_color = color
        self.second_color = second_color
        self.setStyleSheet(f"color: {color};")
        
