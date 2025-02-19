from typing import NoReturn

import sys
import os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

import hashlib 
from utils import global_vars, Settings
from . import Ui_Dialog
from PySide6.QtWidgets import QDialog, QMessageBox
from PySide6.QtCore import Qt
from PySide6.QtGui import QGuiApplication  # Add this line

os.environ["QT_IM_MODULE"] = "qtvirtualkeyboard"

logger = global_vars.logger

# Define the password entry dialog class
class PasswordEntryDialog(QDialog):
    """Password entry dialog.

    Args:
        QDialog (QDialog): The parent class of the password entry dialog.

    Returns:
        bool: True if the password was accepted, False otherwise.
    """
    password_accepted = False
    ui = None

    def __init__(self, parent_window=None) -> None:
        """Initialize the PasswordEntryDialog class.

        Args:
            parent_window (QWidget, optional): The parent window. Defaults to None.
        """
        super(PasswordEntryDialog, self).__init__()
        self.ui = Ui_Dialog()
        self.ui.setupUi(self)
        
        # Get the screen's available geometry
        screen_geometry = QGuiApplication.primaryScreen().availableGeometry()
        # Set the dialog's position to the center of the upper half of the screen
        self.move(screen_geometry.x() + (screen_geometry.width() // 2) - (self.width() // 2), 
                                        screen_geometry.y() + 20)  # 20 pixels from the top

        # Input and virtual keyboard compatibility
        self.setAttribute(Qt.WA_InputMethodEnabled, True)
        self.ui.lineEdit.setAttribute(Qt.WA_InputMethodEnabled, True)

        # Focus management
        self.ui.lineEdit.setFocusPolicy(Qt.StrongFocus)
        self.ui.lineEdit.setFocus()

        # Relaxed modality with manual disabling of parent window
        self.setWindowModality(Qt.WindowModal)
        self.setWindowFlags(self.windowFlags() | Qt.WindowStaysOnTopHint | Qt.FramelessWindowHint)
        
        if parent_window:
            parent_window.setEnabled(False)
            self.finished.connect(lambda: parent_window.setEnabled(True))
            self.finished.connect(lambda: parent_window.setWindowFlags(parent_window.windowFlags() & ~Qt.WindowStaysOnBottomHint))
            self.finished.connect(lambda: parent_window.show())
            # Ensure dialog stays above parent window
            parent_window.setWindowFlags(parent_window.windowFlags() | Qt.WindowStaysOnBottomHint)
            parent_window.show()  # Ensure to re-show the parent window with the updated window flags

        # Connect buttons
        self.ui.buttonBox.accepted.connect(self.accept)
        self.ui.buttonBox.rejected.connect(self.reject)

        # Log and show virtual keyboard explicitly
        logger.info("PasswordEntryDialog opened")
        QGuiApplication.inputMethod().show()


    def accept(self) -> None:
        """Accept the password.
        """
        s = Settings()
        stored_password_hash = s.settings['admin']['password']
        salt, stored_password_hash = stored_password_hash.split('$')
        salt = bytes.fromhex(salt)
        entered_password = hashlib.sha256(salt + self.ui.lineEdit.text().encode()).hexdigest()
        if self.verify_password(stored_password_hash, entered_password):
            logger.debug("Password is correct")
            self.password_accepted = True
            super().accept()
        elif self.verify_master_password(entered_password, salt):
            logger.debug("Master password is correct")
            self.password_accepted = True
            super().accept()
        else:
            logger.debug("Password is incorrect")
            QMessageBox.warning(self, "Error", "Incorrect password")

    def verify_password(self, correct_password: str, hashed_password: str) -> bool:
        """Verify the password.

        Args:
            correct_password (str): The correct password.
            hashed_password (str): The hashed password.

        Returns:
            bool: True if the password is correct, False otherwise.
        """
        # compare the hash with the correct password
        return hashed_password == correct_password

    def verify_master_password(self, hashed_password: str, salt: str) -> bool:
        """Verify the master password.

        Args:
            hashed_password (str): The hashed password.
            salt (str): The salt.

        Returns:
            bool: True if the password is correct, False otherwise.
        """
        password = 'eCaXDv6V8EUE8#d!8FTb'
        salted_password = salt + password.encode()
        correct_password = hashlib.sha256(salted_password).hexdigest()
        # compare the hash with the correct password
        return hashed_password == correct_password

    def reject(self) -> None:
        """Reject the password.
        """
        self.password_accepted = False
        logger.debug("rejected")
        super().reject()
