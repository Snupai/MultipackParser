from PySide6.QtWidgets import QLabel
from PySide6.QtCore import QTimer, QRect, Qt
from PySide6.QtGui import QFont

class BlinkingLabel(QLabel):
    def __init__(self, text, color:str, geometry:QRect, parent=None, second_color:str=None, font:QFont=None, alignment:Qt.AlignmentFlag=None):
        super().__init__(text, parent)
        self.setGeometry(geometry)
        self.setStyleSheet(f"color: {color};")
        if font:
            self.setFont(font)
        if alignment:
            self.setAlignment(alignment)
        if not second_color:
            self.timer = QTimer(self)
            self.timer.timeout.connect(self.toggle_visibility)
            self.timer.start(500)  # Blink every 500 ms
        else:
            colours = [color, second_color]
            self.timer = QTimer(self)
            self.timer.timeout.connect(lambda: self.change_color(colours))
            self.timer.start(500)  # Blink every 500 ms

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
        
