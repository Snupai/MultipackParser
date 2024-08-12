from PySide6.QtWidgets import QApplication, QMainWindow, QDialog, QMessageBox, QWidget
from PySide6.QtCore import Qt, QEvent
import sys
import logging
import subprocess
import argparse
from logging.handlers import RotatingFileHandler
from ui_main_window import Ui_Form  # Import the generated main window class
from ui_password_entry import Ui_Dialog  # Import the generated dialog class
from typing import NoReturn

VERSION = '0.0.1'

# Setup logging
log_formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
log_handler = RotatingFileHandler('app.log', maxBytes=5*1024*1024, backupCount=2)
log_handler.setFormatter(log_formatter)
console_handler = logging.StreamHandler(sys.stdout)
console_handler.setFormatter(log_formatter)

logging.basicConfig(level=logging.DEBUG, handlers=[log_handler, console_handler])
logger = logging.getLogger(__name__)

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
        entered_password = self.ui.lineEdit.text()
        if self.verify_password(entered_password):
            logger.debug("Password is correct")
            self.password_accepted = True
            super().accept()
        else:
            logger.debug("Password is incorrect")
            QMessageBox.warning(self, "Error", "Incorrect password")

    def verify_password(self, password: str) -> bool:
        # Replace 'correct_password' with your actual password check logic
        correct_password = "666666"
        return password == correct_password

    def reject(self) -> NoReturn:
        self.password_accepted = False
        logger.debug("rejected")
        super().reject()


# Define the main window class
class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super(MainWindow, self).__init__()
        self.ui = Ui_Form()
        self.ui.setupUi(self)
        self.ui.stackedWidget.setCurrentIndex(0)
        # Connect the button to open the dialog
        self.ui.settings.clicked.connect(self.open_password_dialog)

        # Page 2
        self.ui.Button_OpenExplorer.clicked.connect(self.open_explorer)
        self.ui.Button_OpenTerminal.clicked.connect(self.open_terminal)

        # Install an event filter for key events
        self.allow_close = False

    ####################
    # Page 0 functions #
    ####################

    def open_password_dialog(self) -> None:
        dialog = PasswordEntryDialog()
        if dialog.exec() == QDialog.Accepted and dialog.password_accepted:
            self.open_another_page()

    def open_another_page(self) -> None:
        # set page of the stacked widgets to index 2
        self.ui.stackedWidget.setCurrentIndex(2)

    ####################
    # Page 1 functions #
    ####################

    # spceholder

    ####################
    # Page 2 functions #
    ####################

    def open_explorer(self) -> None:
        logger.info("Opening explorer")
        if sys.platform == "win32":
            subprocess.Popen(["explorer.exe"])
        elif sys.platform == "linux":
            subprocess.Popen(["xdg-open", "."])

    def open_terminal(self) -> None:
        logger.info("Opening terminal")
        if sys.platform == "win32":
            subprocess.Popen(["start", "cmd.exe"], shell=True)
        elif sys.platform == "linux":
            subprocess.Popen(["gnome-terminal", "--window"])

    ################
    # Event filter #
    ################

    def closeEvent(self, event):
        if self.allow_close:
            event.accept()
        else:
            event.ignore()

    def keyPressEvent(self, event):
        # Check for the key combination Ctrl + Alt + Shift + C
        if (event.modifiers() == (Qt.ControlModifier | Qt.AltModifier | Qt.ShiftModifier) and 
            event.key() == Qt.Key_C):
            self.allow_close = True
            self.close()



def main() -> None:
    parser = argparse.ArgumentParser(description="Multipack Parser Application")
    parser.add_argument('--version', action='store_true', help='Show version information and exit')
    args = parser.parse_args()

    if args.version:
        print(f"Multipack Parser Application Version: {VERSION}")
        return
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()