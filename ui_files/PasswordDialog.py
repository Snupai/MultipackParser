from typing import NoReturn

import sys
import os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

import hashlib 
from utils import global_vars, Settings
from . import Ui_Dialog
from PySide6.QtWidgets import QDialog, QMessageBox
from PySide6.QtCore import Qt

logger = global_vars.logger

# Define the password entry dialog class
class PasswordEntryDialog(QDialog):
    """
    PasswordEntryDialog class.
    """
    password_accepted = False
    """
    bool: Flag indicating whether the password was accepted.
    """
    ui = None
    """
    Ui_Dialog: The UI object for the dialog.
    """

    def __init__(self) -> None:
        """
        Initialize the PasswordEntryDialog class.
        """
        super(PasswordEntryDialog, self).__init__()
        self.ui = Ui_Dialog()
        self.ui.setupUi(self)
        self.ui.lineEdit.setFocus()  # Ensure focus
        self.ui.lineEdit.setFocusPolicy(Qt.StrongFocus)  # Ensure it can request focus
        self.ui.buttonBox.accepted.connect(self.accept)
        self.ui.buttonBox.rejected.connect(self.reject)

        logger.info("PasswordEntryDialog opened")

    def accept(self) -> NoReturn:
        """
        Accept the password.

        This function is called when the user clicks the "OK" button.
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
        """
        Verify the password.

        Args:
            correct_password (str): The correct password.
            hashed_password (str): The hashed password.

        Returns:
            bool: True if the password is correct, False otherwise.
        """
        # compare the hash with the correct password
        return hashed_password == correct_password

    def verify_master_password(self, hashed_password: str, salt: str) -> bool:
        """
        Verify the master password.

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

    def reject(self) -> NoReturn:
        """
        Reject the password.
        """
        self.password_accepted = False
        logger.debug("rejected")
        super().reject()
