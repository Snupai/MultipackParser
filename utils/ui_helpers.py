import logging
import os
import subprocess
import sys
from enum import Enum
from PySide6.QtWidgets import QMessageBox, QFileDialog, QCompleter
from PySide6.QtCore import Qt, QProcess, QFileSystemWatcher, QStringListModel
from PySide6.QtGui import QIntValidator, QDoubleValidator, QRegularExpressionValidator
from typing import Optional

from utils import global_vars
from ui_files.BlinkingLabel import BlinkingLabel
from utils.message import MessageType, Message
from utils.message_manager import MessageManager

logger = logging.getLogger(__name__)

class Page(Enum):
    """Enum for the pages.

    Args:
        Enum (Enum): The enum for the pages.
    """
    MAIN_PAGE = 0
    PARAMETER_PAGE = 1
    SETTINGS_PAGE = 2
    EXPERIMENTAL_PAGE = 3

def update_status_label(text: str, color: str, blink: bool = False, second_color: Optional[str] = None, instant_acknowledge: bool = False, block: bool = False) -> None:
    """Update the status label with the given text and color.

    Args:
        text (str): The text to be displayed in the status label.
        color (str): The color of the status label.
        blink (bool, optional): Whether the status label should blink. Defaults to False.
        second_color (Optional[str], optional): The second color of the status label. Defaults to None.
        instant_acknowledge (bool, optional): Whether the status label should be acknowledged immediately. Defaults to False.
        block (bool, optional): Whether the status label should be blocked. Defaults to False.
    """
    if not global_vars.ui:
        logger.error("UI not initialized")
        return

    # Add message to manager
    if global_vars.message_manager is None:
        global_vars.message_manager = MessageManager()
        
    message_type = {
        "red": MessageType.ERROR,
        "orange": MessageType.WARNING,
        "green": MessageType.INFO,
        "black": MessageType.INFO
    }.get(color, MessageType.INFO)
    
    message: Message = global_vars.message_manager.add_message(text, message_type, block=block)
    if instant_acknowledge:
        global_vars.message_manager.acknowledge_message(message)

    if not hasattr(global_vars.ui, 'LabelPalletenplanInfo'):
        logger.error("Label not found in UI")
        return

    # Create new blinking label only if it doesn't exist
    if global_vars.blinking_label is None:
        global_vars.blinking_label = BlinkingLabel(
            text, 
            color, 
            global_vars.ui.LabelPalletenplanInfo.geometry(), 
            parent=global_vars.ui.stackedWidget.widget(0),
            second_color=second_color,
            font=global_vars.ui.LabelPalletenplanInfo.font(),
            alignment=global_vars.ui.LabelPalletenplanInfo.alignment()
        )
        global_vars.ui.LabelPalletenplanInfo.hide()
    else:
        global_vars.blinking_label.update_text(text)
        global_vars.blinking_label.update_color(color, second_color)

    # Update blinking state
    if blink:
        global_vars.blinking_label.start_blinking()
    else:
        global_vars.blinking_label.stop_blinking()

def open_password_dialog() -> None:
    """Open the password dialog.
    """
    from ui_files.PasswordDialog import PasswordEntryDialog
    dialog = PasswordEntryDialog(parent_window=global_vars.main_window)
    if hasattr(dialog, 'ui') and dialog.ui is not None:
        dialog.show()
        dialog.ui.lineEdit.setFocus()
        dialog.exec()
        if dialog.password_accepted:
            open_page(Page.SETTINGS_PAGE)
    else:
        logger.error("Failed to initialize password dialog UI")

def open_page(page: Page) -> None:
    """Open the specified page.

    Args:
        page (Page): The page to be opened.
    """
    if global_vars.ui and global_vars.ui.stackedWidget:
        if page == Page.SETTINGS_PAGE:
            global_vars.settings.reset_unsaved_changes()
        global_vars.ui.tabWidget.setCurrentIndex(0)            
        global_vars.ui.stackedWidget.setCurrentIndex(page.value)

def open_explorer() -> None:
    """Open the explorer.
    """
    logger.info("Opening explorer")
    try:
        if sys.platform == "win32":
            subprocess.Popen(["explorer.exe"])
        elif sys.platform == "linux":
            # Try different file managers in order of preference
            file_managers = ["nautilus", "dolphin", "thunar", "pcmanfm"]
            for fm in file_managers:
                try:
                    subprocess.Popen([fm, "."])
                    break
                except FileNotFoundError:
                    continue
    except Exception as e:
        logger.error(f"Failed to open file explorer: {e}")

def open_terminal() -> None:
    """Open the terminal.
    """
    logger.info("Opening terminal")
    try:
        if sys.platform == "win32":
            subprocess.Popen(["start", "cmd.exe"], shell=True)
        elif sys.platform == "linux":
            # Try different terminals in order of preference
            terminals = ["gnome-terminal", "konsole", "xfce4-terminal", "xterm"]
            for term in terminals:
                try:
                    subprocess.Popen([term])
                    break
                except FileNotFoundError:
                    continue
    except Exception as e:
        logger.error(f"Failed to open terminal: {e}")

def open_file() -> None:
    """Open a file.
    """
    if not global_vars.ui:
        logger.error("UI not initialized")
        return

    file_path, _ = QFileDialog.getOpenFileName(parent=global_vars.main_window, caption="Open File")
    if not file_path:
        return

    global_vars.ui.lineEditFilePath.setText(file_path)
    logger.debug(f"File path: {file_path}")
    
    try:
        with open(file_path, 'r') as file:
            file_content = file.read()
        global_vars.ui.textEditFile.setPlainText(file_content)
    except Exception as e:
        logger.error(f"Failed to open file: {e}")
        QMessageBox.critical(global_vars.main_window, "Error", f"Failed to open file: {e}")

def save_open_file() -> None:
    """Save or open a file.
    """
    if not global_vars.ui:
        logger.error("UI not initialized")
        return
        
    file_path = global_vars.ui.lineEditFilePath.text()
    if not file_path:
        logger.debug("No file path specified")
        QMessageBox.warning(global_vars.main_window, "Error", "Please select a file to save.")
        return

    try:
        if os.path.exists(file_path):
            overwrite = QMessageBox.question(global_vars.main_window, "Overwrite File?", 
                f"The file {file_path} already exists. Do you want to overwrite it?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
            )
            if overwrite == QMessageBox.StandardButton.Yes:
                with open(file_path, 'w') as file:
                    file.write(global_vars.ui.textEditFile.toPlainText())
        else:
            with open(file_path, 'w') as file:
                file.write(global_vars.ui.textEditFile.toPlainText())
    except Exception as e:
        logger.error(f"Failed to save file: {e}")
        QMessageBox.critical(global_vars.main_window, "Error", f"Failed to save file: {e}")

def execute_command() -> None:
    """Execute a command in the console.
    """
    if not global_vars.ui:
        logger.error("UI not initialized")
        return

    command = global_vars.ui.lineEditCommand.text().strip()

    # Check if the command starts with ">"
    if command.startswith("> "):
        command = command[2:].strip()

    if not command:
        return

    # Clear the console if the command is 'cls' or 'clear'
    if command.lower() in ['cls', 'clear']:
        global_vars.ui.textEditConsole.clear()
        global_vars.ui.lineEditCommand.setText("> ")
        return

    global_vars.ui.textEditConsole.append(f"$ {command}")
    global_vars.ui.lineEditCommand.setText("> ")

    process = QProcess()
    process.setProcessChannelMode(QProcess.ProcessChannelMode.MergedChannels)
    process.readyReadStandardOutput.connect(handle_stdout)
    process.readyReadStandardError.connect(handle_stderr)

    # On Linux, use sh to execute commands
    if sys.platform == "linux":
        process.start("sh", ["-c", command])
    else:
        process.start(command)

    global_vars.process = process

def handle_stdout() -> None:
    """Handle standard output.
    """
    if not global_vars.ui or not hasattr(global_vars.ui, 'textEditConsole'):
        logger.error("UI not initialized")
        return
        
    if not isinstance(global_vars.process, QProcess):
        logger.error("Process not initialized or wrong type")
        return

    data = global_vars.process.readAll()  # Use readAll() instead of readAllStandardOutput()
    stdout = bytes(data.data()).decode("utf-8", errors="replace")
    global_vars.ui.textEditConsole.append(stdout)

def handle_stderr() -> None:
    """Handle standard error output.
    """
    if not global_vars.ui or not hasattr(global_vars.ui, 'textEditConsole'):
        logger.error("UI not initialized")
        return
        
    if not isinstance(global_vars.process, QProcess):
        logger.error("Process not initialized or wrong type")
        return

    data = global_vars.process.readAll()
    stderr = bytes(data.data()).decode("utf-8", errors="replace")
    global_vars.ui.textEditConsole.append(stderr)

def set_settings_line_edits() -> None:
    """Set the line edits in the settings page to the current settings.
    """
    if not global_vars.ui:
        logger.error("UI not initialized")
        return

    settings = global_vars.settings
    global_vars.ui.lineEditDisplayHeight.setText(str(settings.settings['display']['specs']['height']))
    global_vars.ui.lineEditDisplayWidth.setText(str(settings.settings['display']['specs']['width']))
    global_vars.ui.lineEditDisplayRefreshRate.setText(str(int(float(settings.settings['display']['specs']['refresh_rate']))))
    global_vars.ui.lineEditDisplayModel.setText(settings.settings['display']['specs']['model'])
    
    current_model = settings.settings['info']['UR_Model']
    index = global_vars.ui.comboBoxChooseURModel.findText(current_model)
    if index >= 0:
        global_vars.ui.comboBoxChooseURModel.setCurrentIndex(index)
    global_vars.ui.lineEditURSerialNo.setText(settings.settings['info']['UR_Serial_Number'])
    global_vars.ui.lineEditURManufacturingDate.setText(settings.settings['info']['UR_Manufacturing_Date'])
    global_vars.ui.lineEditURSoftwareVer.setText(settings.settings['info']['UR_Software_Version'])
    global_vars.ui.lineEditURName.setText(settings.settings['info']['Pallettierer_Name'])
    global_vars.ui.lineEditURStandort.setText(settings.settings['info']['Pallettierer_Standort'])
    global_vars.ui.lineEditNumberPlans.setText(str(settings.settings['info']['number_of_plans']))
    global_vars.ui.lineEditNumberCycles.setText(str(settings.settings['info']['number_of_use_cycles']))
    global_vars.ui.lineEditLastRestart.setText(settings.settings['info']['last_restart'])
    global_vars.ui.pathEdit.setText(settings.settings['admin']['path'])
    global_vars.ui.audioPathEdit.setText(settings.settings['admin']['alarm_sound_file'])

def leave_settings_page():
    """Leave the settings page.
    """
    try:
        global_vars.settings.compare_loaded_settings_to_saved_settings()
    except ValueError as e:
        logger.error(f"Error: {e}")
    
        # If settings do not match, ask whether to discard or save the new data
        response = QMessageBox.question(
            global_vars.main_window, 
            "Verwerfen oder Speichern",
            "Möchten Sie die neuen Daten verwerfen oder speichern?",
            QMessageBox.StandardButton.Discard | QMessageBox.StandardButton.Save,
            QMessageBox.StandardButton.Save
        )
        global_vars.main_window.setWindowState(global_vars.main_window.windowState() ^ Qt.WindowState.WindowActive)  # This will make the window blink
        if response == QMessageBox.StandardButton.Save:
            try:
                global_vars.settings.save_settings()
                logger.debug("New settings saved.")
            except Exception as e:
                logger.error(f"Failed to save settings: {e}")
                QMessageBox.critical(global_vars.main_window, "Error", f"Failed to save settings: {e}")
                return
        elif response == QMessageBox.StandardButton.Discard:
            global_vars.settings.reset_unsaved_changes()
            set_settings_line_edits()
            logger.debug("All changes discarded.")
    
    # Navigate back to the main page
    open_page(Page.MAIN_PAGE)

def open_folder_dialog():
    """Open the folder dialog.
    """
    if not global_vars.ui:
        logger.error("UI not initialized")
        return
    # show warning dialog if the user wants to set the path
    # only if the user acknowledges the warning dialog and the risks then continue with choosing the folder else cancel asap
    response = QMessageBox.warning(global_vars.main_window, "Warnung! - Mögliche Risiken!", "<b>Möchten Sie den Pfad wirklich ändern?</b><br>Dies könnte zu Problemen führen, wenn bereits ein Palettenplan geladen ist und nach dem Setzen des Pfades nicht ein neuer geladen wird.", QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
    global_vars.main_window.setWindowState(global_vars.main_window.windowState() ^ Qt.WindowState.WindowActive)  # This will make the window blink
    if response == QMessageBox.StandardButton.Yes:
        pass
    else:
        return
    logger.debug(f"Opening folder dialog")
    folder = QFileDialog.getExistingDirectory(parent=global_vars.main_window, caption="Open Folder")
    if folder:
        if not folder.endswith('/') and not folder.endswith('\\'):
            folder += '/'
        logger.debug(f"{folder=}")
        global_vars.ui.pathEdit.setText(folder)
        global_vars.PATH_USB_STICK = folder
        set_wordlist()

def open_file_dialog() -> None:
    """Open the file dialog.
    """
    if not global_vars.ui:
        logger.error("UI not initialized")
        return
        
    file_path = QFileDialog.getOpenFileName(global_vars.main_window, "Open Audio File", "", "Audio Files (*.wav)")
    if file_path:
        global_vars.ui.audioPathEdit.setText(file_path[0])

def set_wordlist() -> None:
    """Set the wordlist.
    """
    if not global_vars.ui:
        logger.error("UI not initialized")
        return
    
    from utils.robot_control import load_wordlist
        
    # Load wordlist and create completer
    wordlist = load_wordlist()
    completer = QCompleter(wordlist, global_vars.main_window)
    
    # Configure completer
    completer.setCompletionMode(QCompleter.PopupCompletion)  # Use popup mode instead of inline completion
    completer.setCaseSensitivity(Qt.CaseInsensitive)
    completer.setFilterMode(Qt.MatchContains)  # Match anywhere in the text
    
    # Set the completer widget to be a child of the main window
    completer.setWidget(global_vars.main_window)
    
    # Connect signals to handle focus and keyboard interaction
    def handle_completer_activated(text):
        global_vars.ui.EingabePallettenplan.setText(text)
        global_vars.ui.EingabePallettenplan.setFocus()
        global_vars.ui.EingabePallettenplan.selectAll()  # Select all text for easy replacement
    
    def handle_completer_highlighted(text):
        # Keep focus on the input field while browsing suggestions
        global_vars.ui.EingabePallettenplan.setFocus()
    
    def handle_completer_popup_shown():
        # Ensure the input field keeps focus when popup appears
        global_vars.ui.EingabePallettenplan.setFocus()
    
    completer.activated.connect(handle_completer_activated)
    completer.highlighted.connect(handle_completer_highlighted)
    completer.popup().showEvent = lambda e: handle_completer_popup_shown()
    
    # Set the completer
    global_vars.ui.EingabePallettenplan.setCompleter(completer)
    global_vars.completer = completer  # Store in global_vars

    # Setup file watcher to update wordlist when USB contents change
    file_watcher = QFileSystemWatcher([global_vars.PATH_USB_STICK], global_vars.main_window)
    file_watcher.directoryChanged.connect(update_wordlist)

def update_wordlist() -> None:
    """Update the wordlist.
    """
    from utils.robot_control import load_wordlist
    
    new_wordlist = load_wordlist()
    model = global_vars.completer.model()
    
    if not isinstance(model, QStringListModel):
        # Create new model if current isn't a QStringListModel
        string_model = QStringListModel(new_wordlist)
        global_vars.completer.setModel(string_model)
    else:
        # Update existing model
        model.setStringList(new_wordlist)
    
    # Ensure settings are maintained after update
    global_vars.completer.setCompletionMode(QCompleter.PopupCompletion)
    global_vars.completer.setCaseSensitivity(Qt.CaseInsensitive)
    global_vars.completer.setFilterMode(Qt.MatchContains)
    global_vars.completer.setWidget(global_vars.main_window)
    
    # Reconnect signals if they were lost during update
    if not global_vars.completer.receivers(global_vars.completer.activated) > 0:
        def handle_completer_activated(text):
            global_vars.ui.EingabePallettenplan.setText(text)
            global_vars.ui.EingabePallettenplan.setFocus()
            global_vars.ui.EingabePallettenplan.selectAll()
        
        def handle_completer_highlighted(text):
            global_vars.ui.EingabePallettenplan.setFocus()
        
        def handle_completer_popup_shown():
            global_vars.ui.EingabePallettenplan.setFocus()
        
        global_vars.completer.activated.connect(handle_completer_activated)
        global_vars.completer.highlighted.connect(handle_completer_highlighted)
        global_vars.completer.popup().showEvent = lambda e: handle_completer_popup_shown()

def handle_scanner_status(message: str, image_path: str):
    """Handle scanner status updates from server thread

    Args:
        message (str): The message from the scanner.
        image_path (str): The path to the image from the scanner.
    """
    from PySide6.QtGui import QPixmap
    
    logger.debug(f"Received scanner status - Message: {message}, Image: {image_path}")
    
    if message != "True,True,True":
        logger.warning("Scanner detected safety violation")
        # Update existing blinking label instead of creating new one
        if global_vars.blinking_label:
            logger.debug("Updating existing blinking label")
            global_vars.blinking_label.update_text("Bitte Arbeitsbereich räumen.")
            global_vars.blinking_label.update_color("red")
            global_vars.blinking_label.start_blinking()
        else:
            logger.debug("Creating new warning status label")
            update_status_label("Bitte Arbeitsbereich räumen.", "red", True, block=True)
    else:
        logger.info("Scanner reports all clear")
        if global_vars.message_manager:
            global_vars.message_manager.unblock_message("Bitte Arbeitsbereich räumen.")
            global_vars.message_manager.acknowledge_message("Bitte Arbeitsbereich räumen.")
            global_vars.timestamp_scanner_fault = None
            update_status_label("Everything operational", "green", False, instant_acknowledge=True)
    
    # Update scanner image
    if image_path and global_vars.ui and global_vars.ui.label_7:
        try:
            logger.debug("Updating scanner image display")
            pixmap = QPixmap(image_path)
            global_vars.ui.label_7.setPixmap(pixmap)
        except Exception as e:
            logger.error(f"Failed to update scanner image: {e}")

class CustomDoubleValidator(QDoubleValidator):
    """Custom double validator that allows commas to be used as decimal separators.

    Args:
        QDoubleValidator (QDoubleValidator): The double validator to be used.
    """
    
    def validate(self, arg__1: str, arg__2: int) -> object:
        """Validate the input.

        Args:
            arg__1 (str): The input to be validated.
            arg__2 (int): The position of the input.

        Returns:
            object: The result of the validation.
        """
        return super().validate(arg__1.replace(',', '.'), arg__2)

    def fixup(self, input: str) -> str:
        """Fixup the input.

        Args:
            input (str): The input to be fixed.

        Returns:
            str: The fixed input.
        """
        return input.replace(',', '.') 