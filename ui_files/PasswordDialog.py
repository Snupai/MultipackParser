from typing import NoReturn

import sys
import os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

import hashlib 
from utils import global_vars
from . import Ui_Dialog
from PySide6.QtWidgets import QDialog, QMessageBox

logger = global_vars.logger

# Define the password entry dialog class
class PasswordEntryDialog(QDialog):
    password_accepted = False

    def __init__(self) -> None:
        super(PasswordEntryDialog, self).__init__()
        self.ui = Ui_Dialog()
        self.ui.setupUi(self)

        self.ui.buttonBox.accepted.connect(self.accept)
        self.ui.buttonBox.rejected.connect(self.reject)

        logger.info("PasswordEntryDialog opened")

    def accept(self) -> NoReturn:
        entered_password = hashlib.sha256(self.ui.lineEdit.text().encode()).hexdigest()
        if self.verify_password(entered_password):
            logger.debug("Password is correct")
            self.password_accepted = True
            super().accept()
        else:
            logger.debug("Password is incorrect")
            QMessageBox.warning(self, "Error", "Incorrect password")

    def verify_password(self, hashed_password: str) -> bool:
        correct_password = "94edf28c6d6da38fd35d7ad53e485307f89fbeaf120485c8d17a43f323deee71"  # SHA256 hash of "password"
        # compare the hash with the correct password
        return hashed_password == correct_password

    def reject(self) -> NoReturn:
        self.password_accepted = False
        logger.debug("rejected")
        super().reject()