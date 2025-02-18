__license__ = '''
    Multipack Parser Application - to parse the data from the Multipack Robot to an UR Robot
    Copyright (C) 2024  Yann-Luca Näher

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
'''

#TODO: After starting the program, ask the user to confirm each palette if it is empty or not. and if it is not empty ask the user to confirm if the user wants to continue anyways and ask for the current layer.
#TODO: Implement option for UR10e or UR20 robot. If UR20 is selected robot will have 2 pallets. else only it is like the old code.
#TODO: Implement seemless palletizing with 2 pallets for UR20 robot.

import sys
import subprocess
import argparse
import hashlib
import os
import shutil
import time
import threading
import socket
from xmlrpc.server import SimpleXMLRPCServer
import logging
from datetime import datetime
from typing import Literal
from enum import Enum

# import the needed qml modules for the virtual keyboard to work
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtQuick import QQuickView
################################################################
from PySide6.QtWidgets import QApplication, QMainWindow, QMessageBox, QCompleter, QFileDialog, QMessageBox
from PySide6.QtCore import Qt, QFileSystemWatcher, QProcess, QRegularExpression, QLocale, QStringListModel
from PySide6.QtGui import QIntValidator, QDoubleValidator, QRegularExpressionValidator, QIcon
from ui_files.ui_main_window import Ui_Form
from ui_files import MainWindowResources_rc
from ui_files.BlinkingLabel import BlinkingLabel
from utils.settings import Settings
from utils import global_vars
from utils import UR_Common_functions as UR
from utils import UR10_Server_functions as UR10
from utils import UR20_Server_functions as UR20

logger = global_vars.logger

audio_thread = None
audio_thread_running = False

os.environ["QT_IM_MODULE"] = "qtvirtualkeyboard"

# global_vars.PATH_USB_STICK = 'E:\' # Pfad zu den .rob Dateien nur auskommentieren zum testen

if global_vars.PATH_USB_STICK == '..':
    # Bekomme CWD und setze den Pfad auf den Überordner
    logger.debug(os.path.dirname(os.getcwd()))
    global_vars.PATH_USB_STICK = f'{os.path.dirname(os.getcwd())}/' 

####################
# Server functions #
####################

def server_start() -> Literal[0]:
    """
    Start the XMLRPC server.

    Returns:
        0
    """
    global server
    server = SimpleXMLRPCServer(("", 8080), allow_none=True)
    logger.debug("Start Server")

    try:
        robot_type = settings.settings['info']['UR_Model']  # Use the global settings object instead
        logger.debug(f"Robot type: {robot_type}")
        if robot_type not in ['UR10', 'UR20']:
            # default to UR10
            robot_type = 'UR10'
            logger.warning(f"Invalid robot type {robot_type}, defaulting to UR10")
    except (AttributeError, KeyError, TypeError) as e:
        # If there's any error accessing the settings, default to UR10
        robot_type = 'UR10'
        logger.error(f"Error accessing robot type from settings: {e}. Defaulting to UR10")
        
    # Register common functions for both robot types
    server.register_function(UR.UR_SetFileName, "UR_SetFileName")
    server.register_function(UR.UR_ReadDataFromUsbStick, "UR_ReadDataFromUsbStick")
    server.register_function(UR.UR_Palette, "UR_Palette")
    server.register_function(UR.UR_Karton, "UR_Karton")
    server.register_function(UR.UR_Lagen, "UR_Lagen")
    server.register_function(UR.UR_Zwischenlagen, "UR_Zwischenlagen")
    server.register_function(UR.UR_PaketPos, "UR_PaketPos")
    server.register_function(UR.UR_AnzLagen, "UR_AnzLagen")
    server.register_function(UR.UR_AnzPakete, "UR_AnzPakete")
    server.register_function(UR.UR_PaketeZuordnung, "UR_PaketeZuordnung")
    server.register_function(UR.UR_Paket_hoehe, "UR_Paket_hoehe")
    server.register_function(UR.UR_Startlage, "UR_Startlage")
    server.register_function(UR.UR_Quergreifen, "UR_Quergreifen")
    server.register_function(UR.UR_CoG, "UR_CoG")
    server.register_function(UR.UR_MasseGeschaetzt, "UR_MasseGeschaetzt")
    server.register_function(UR.UR_PickOffsetX, "UR_PickOffsetX")
    server.register_function(UR.UR_PickOffsetY, "UR_PickOffsetY")
    
    
    # Register robot type specific functions here if needed
    if robot_type == 'UR10':
        server.register_function(UR10.UR10_scanner1and2niobild, "UR_scanner1and2niobild")
        server.register_function(UR10.UR10_scanner1bild, "UR_scanner1bild")
        server.register_function(UR10.UR10_scanner2bild, "UR_scanner2bild")
        server.register_function(UR10.UR10_scanner1and2iobild, "UR_scanner1and2iobild")
    elif robot_type == 'UR20':
        server.register_function(UR20.UR20_scannerStatus, "UR_scannerStatus") # type: ignore
        server.register_function(UR20.UR20_SetActivePalette, "UR_SetActivePalette")
        server.register_function(UR20.UR20_GetActivePaletteNumber, "UR_GetActivePaletteNumber")
        server.register_function(UR20.UR20_GetPaletteStatus, "UR_GetPaletteStatus")
        
    logger.debug(f"Successfully registered functions for {robot_type}")
    
    server.serve_forever()
    return 0

def server_stop() -> None:
    """
    Stop the XMLRPC server.
    """
    if global_vars.ui and global_vars.ui.ButtonStopRPCServer:
        global_vars.ui.ButtonStopRPCServer.setEnabled(False)
    server.shutdown()
    logger.debug("Server stopped")
    datensenden_manipulation(True, "Server starten", "")

def server_thread() -> None:
    """
    Start the XMLRPC server in a separate thread.
    """
    logger.debug("Starting server thread")
    xServerThread = threading.Thread(target=server_start)
    xServerThread.start()
    if global_vars.ui and global_vars.ui.ButtonStopRPCServer:
        global_vars.ui.ButtonStopRPCServer.setEnabled(True)
    datensenden_manipulation(False, "Server läuft", "green")

def datensenden_manipulation(visibility: bool, display_text: str, display_colour: str) -> None:
    """Manipulate the visibility of the "Daten Senden" button and the display text."""
    if global_vars.ui:
        buttons = []
        if hasattr(global_vars.ui, 'ButtonDatenSenden'):
            buttons.append(global_vars.ui.ButtonDatenSenden)
        if hasattr(global_vars.ui, 'ButtonDatenSenden_2'):
            buttons.append(global_vars.ui.ButtonDatenSenden_2)
            
        for button in buttons:
            button.setStyleSheet(f"color: {display_colour}")
            button.setEnabled(visibility)
            button.setText(display_text)

def send_cmd_play() -> None:
    """
    Send a command to the robot to start.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        # Connect the socket to the port where the server is listening
        server_address = (global_vars.robot_ip, 29999)
        logger.debug('connecting to %s port %s' %(server_address))
        sock.connect(server_address)
        
        # Send data
        message = 'play\n'
        logger.debug('sending %s' %(message))
        sock.sendall(message.encode('utf-8'))
        
        # Print any response
        data = sock.recv(4096)
        logger.debug('received %s' %(data))
        
    finally:
        logger.debug('closing socket')
        sock.close()

def send_cmd_pause() -> None:
    """
    Send a command to the robot to pause.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        # Connect the socket to the port where the server is listening
        server_address = (global_vars.robot_ip, 29999)
        logger.debug('connecting to %s port %s' %(server_address))
        sock.connect(server_address)
        
        # Send data
        message = 'pause\n'
        logger.debug('sending %s' %(message))
        sock.sendall(message.encode('utf-8'))
        
        # Print any response
        data = sock.recv(4096)
        logger.debug('received %s' %(data))
        
    finally:
        logger.debug('closing socket')
        sock.close()

def send_cmd_stop() -> None:
    """
    Send a command to the robot to stop.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        # Connect the socket to the port where the server is listening
        server_address = (global_vars.robot_ip, 29999)
        logger.debug('connecting to %s port %s' %(server_address))
        sock.connect(server_address)
        
        # Send data
        message = 'stop\n'
        logger.debug('sending %s' %(message))
        sock.sendall(message.encode('utf-8'))
        
        # Print any response
        data = sock.recv(4096)
        logger.debug('received %s' %(data))
        
    finally:
        logger.debug('closing socket')
        sock.close()

################
# UI functions #
################

def update_status_label(text: str, color: str, blink: bool = False, second_color: str | None = None) -> None:
    """Update the status label with the given text and color"""
    if not global_vars.ui:
        logger.error("UI not initialized")
        return

    if not hasattr(global_vars.ui, 'LabelPalletenplanInfo'):
        logger.error("Label not found in UI")
        return

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

    # Update existing blinking label
    if global_vars.blinking_label is not None:
        global_vars.blinking_label.update_text(text)
        global_vars.blinking_label.update_color(color, second_color)
        
        if blink:
            global_vars.blinking_label.start_blinking()
        else:
            global_vars.blinking_label.stop_blinking()

class Page(Enum):
    MAIN_PAGE = 0
    PARAMETER_PAGE = 1
    SETTINGS_PAGE = 2

def open_password_dialog() -> None:
    """
    Open the password dialog.
    """
    from ui_files.PasswordDialog import PasswordEntryDialog
    dialog = PasswordEntryDialog(parent_window=main_window)
    if hasattr(dialog, 'ui') and dialog.ui is not None:
        dialog.show()
        dialog.ui.lineEdit.setFocus()
        dialog.exec()
        if dialog.password_accepted:
            open_page(Page.SETTINGS_PAGE)
    else:
        logger.error("Failed to initialize password dialog UI")

def open_page(page: Page) -> None:
    """
    Open the specified page.
    """
    if global_vars.ui and global_vars.ui.stackedWidget:
        if page == Page.SETTINGS_PAGE:
            settings.reset_unsaved_changes()
        global_vars.ui.tabWidget.setCurrentIndex(0)            
        global_vars.ui.stackedWidget.setCurrentIndex(page.value)

def open_explorer() -> None:
    """
    Open the explorer.
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
    """
    Open the terminal.
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

def load() -> None:
    """Load the pallet plan from file."""
    if not global_vars.ui or not hasattr(global_vars.ui, 'EingabePallettenplan'):
        logger.error("UI not initialized")
        return

    Artikelnummer = global_vars.ui.EingabePallettenplan.text()
    UR.UR_SetFileName(Artikelnummer)
    
    errorReadDataFromUsbStick = UR.UR_ReadDataFromUsbStick()
    if errorReadDataFromUsbStick:
        logger.error(f"Error reading file for {Artikelnummer=} no file found")
        update_status_label("Kein Plan gefunden", "red", True)
        return

    # Enable UI elements and update values only if UI exists
    if global_vars.ui:
        global_vars.ui.EingabePallettenplan.clearFocus()
        logger.debug(f"File for {Artikelnummer=} found")
        update_status_label("Plan erfolgreich geladen", "green")
        global_vars.ui.ButtonOpenParameterRoboter.setEnabled(True)
        global_vars.ui.ButtonDatenSenden.setEnabled(True)
        global_vars.ui.EingabeKartonGewicht.setEnabled(True)
        global_vars.ui.EingabeKartonhoehe.setEnabled(True)
        global_vars.ui.EingabeStartlage.setEnabled(True)
        global_vars.ui.checkBoxEinzelpaket.setEnabled(True)

        if global_vars.g_PaketDim is None:
            logger.error("Package dimensions not initialized")
            return

        Volumen = (global_vars.g_PaketDim[0] * global_vars.g_PaketDim[1] * global_vars.g_PaketDim[2]) / 1E+9 # in m³
        logger.debug(f"{Volumen=}")
        Dichte = 1000 # Dichte von Wasser in kg/m³
        logger.debug(f"{Dichte=}")
        Ausnutzung = 0.4 # Empirsch ermittelter Faktor - nicht für Gasflaschen
        logger.debug(f"{Ausnutzung=}")
        Gewicht = round(Volumen * Dichte * Ausnutzung, 1) # Gewicht in kg
        logger.debug(f"{Gewicht=}")
        global_vars.ui.EingabeKartonGewicht.setText(str(Gewicht))
        global_vars.ui.EingabeKartonhoehe.setText(str(global_vars.g_PaketDim[2]))

def send_data() -> None:
    """
    Send the data to the robot.

    This function is called when the user clicks the "Daten Senden" button.
    """
    logger.debug("Button Daten Senden clicked")
    server_thread()

def load_wordlist() -> list:
    """
    Load the wordlist from the USB stick.

    Returns:
        A list of wordlist items.
    """
    wordlist = []
    count = 0
    for file in os.listdir(global_vars.PATH_USB_STICK):
        if file.endswith(".rob"):
            wordlist.append(file[:-4])
            count = count + 1
    logger.debug(f"Wordlist {count=}")
    settings.settings['info']['number_of_plans'] = count
    return wordlist

def init_settings():
    """Initialize the settings"""
    global settings
    settings = Settings()
    global_vars.PATH_USB_STICK = settings.settings['admin']['path']
    logger.debug(f"Settings initialized: {settings}")

def leave_settings_page():
    """
    Leave the settings page.
    
    This function is called when the user clicks the "Zurueck" button in the settings page.
    """
    try:
        settings.compare_loaded_settings_to_saved_settings()
    except ValueError as e:
        logger.error(f"Error: {e}")
    
        # If settings do not match, ask whether to discard or save the new data
        response = QMessageBox.question(
            main_window, 
            "Verwerfen oder Speichern",
            "Möchten Sie die neuen Daten verwerfen oder speichern?",
            QMessageBox.StandardButton.Discard | QMessageBox.StandardButton.Save,
            QMessageBox.StandardButton.Save
        )
        main_window.setWindowState(main_window.windowState() ^ Qt.WindowState.WindowActive)  # This will make the window blink
        if response == QMessageBox.StandardButton.Save:
            try:
                settings.save_settings()
                logger.debug("New settings saved.")
            except Exception as e:
                logger.error(f"Failed to save settings: {e}")
                QMessageBox.critical(main_window, "Error", f"Failed to save settings: {e}")
                return
        elif response == QMessageBox.StandardButton.Discard:
            settings.reset_unsaved_changes()
            set_settings_line_edits()
            logger.debug("All changes discarded.")
    
    # Navigate back to the main page
    open_page(Page.MAIN_PAGE)

def open_file() -> None:
    """Open a file."""
    if not global_vars.ui:
        logger.error("UI not initialized")
        return

    file_path, _ = QFileDialog.getOpenFileName(parent=main_window, caption="Open File")
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
        QMessageBox.critical(main_window, "Error", f"Failed to open file: {e}")

def save_open_file() -> None:
    """Save or open a file."""
    if not global_vars.ui:
        logger.error("UI not initialized")
        return
        
    file_path = global_vars.ui.lineEditFilePath.text()
    if not file_path:
        logger.debug("No file path specified")
        QMessageBox.warning(main_window, "Error", "Please select a file to save.")
        return

    try:
        if os.path.exists(file_path):
            overwrite = QMessageBox.question(main_window, "Overwrite File?", 
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
        QMessageBox.critical(main_window, "Error", f"Failed to save file: {e}")

def execute_command() -> None:
    """Execute a command in the console."""
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
    """Handle standard output."""
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
    """Handle standard error output."""
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
    """Set the line edits in the settings page to the current settings."""
    if not global_vars.ui:
        logger.error("UI not initialized")
        return

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

def restart_app():
    """
    Restart the system.
    """
    try:
        settings.compare_loaded_settings_to_saved_settings()
    except ValueError as e:
        logger.error(f"Error: {e}")
        response = QMessageBox.question(main_window, "Verwerfen oder Speichern", 
                                            "Möchten Sie die neuen Daten verwerfen oder speichern?",
                                            QMessageBox.StandardButton.Discard | QMessageBox.StandardButton.Save, 
                                            QMessageBox.StandardButton.Save
                                            )
        if response == QMessageBox.StandardButton.Save:
            try:
                settings.save_settings()
            except Exception as e:
                logger.error(f"Failed to save settings: {e}")
                return
    
    logger.info("Rebooting system...")
    if 'server' in globals():
        server_stop()
    subprocess.run(['sudo', 'reboot'], check=True)

def save_and_exit_app():
    """
    Safely exit the application.
    """
    try:
        settings.compare_loaded_settings_to_saved_settings()
    except ValueError as e:
        logger.error(f"Error: {e}")
    
        # If settings do not match, ask whether to discard or save the new data
        response = QMessageBox.question(main_window, "Verwerfen oder Speichern", 
                                            "Möchten Sie die neuen Daten verwerfen oder speichern?",
                                            QMessageBox.StandardButton.Discard | QMessageBox.StandardButton.Save, 
                                            QMessageBox.StandardButton.Save)
        main_window.setWindowState(main_window.windowState() ^ Qt.WindowState.WindowActive)  # This will make the window blink
        if response == QMessageBox.StandardButton.Save:
            try:
                settings.save_settings()
                logger.debug("New settings saved.")
            except Exception as e:
                logger.error(f"Failed to save settings: {e}")
                QMessageBox.critical(main_window, "Error", f"Failed to save settings: {e}")
                return
        elif response == QMessageBox.StandardButton.Discard:
            settings.reset_unsaved_changes()
            set_settings_line_edits()
            logger.debug("All changes discarded.")
    exit_app()

def exit_app():
    """
    Exit the application.
    """
    if 'server' in globals():
        server_stop()
    sys.exit(0)

def set_wordlist() -> None:
    """Set the wordlist."""
    if not global_vars.ui:
        logger.error("UI not initialized")
        return
        
    global completer  # Declare completer as global
    wordlist = load_wordlist()
    completer = QCompleter(wordlist, main_window)
    global_vars.ui.EingabePallettenplan.setCompleter(completer)

    file_watcher = QFileSystemWatcher([global_vars.PATH_USB_STICK], main_window)
    file_watcher.directoryChanged.connect(update_wordlist)

def open_folder_dialog():
    """
    Open the folder dialog.
    """
    if not global_vars.ui:
        logger.error("UI not initialized")
        return
    # show warning dialog if the user wants to set the path
    # only if the user acknowledges the warning dialog and the risks then continue with choosing the folder else cancel asap
    response = QMessageBox.warning(main_window, "Warnung! - Mögliche Risiken!", "<b>Möchten Sie den Pfad wirklich ändern?</b><br>Dies könnte zu Problemen führen, wenn bereits ein Palettenplan geladen ist und nach dem Setzen des Pfades nicht ein neuer geladen wird.", QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
    main_window.setWindowState(main_window.windowState() ^ Qt.WindowState.WindowActive)  # This will make the window blink
    if response == QMessageBox.StandardButton.Yes:
        pass
    else:
        return
    logger.debug(f"Opening folder dialog")
    folder = QFileDialog.getExistingDirectory(parent=main_window, caption="Open Folder")
    if folder:
        if not folder.endswith('/') and not folder.endswith('\\'):
            folder += '/'
        logger.debug(f"{folder=}")
        global_vars.ui.pathEdit.setText(folder)
        global_vars.PATH_USB_STICK = folder
        set_wordlist()

def open_file_dialog() -> None:
    """Open the file dialog."""
    if not global_vars.ui:
        logger.error("UI not initialized")
        return
        
    file_path = QFileDialog.getOpenFileName(main_window, "Open Audio File", "", "Audio Files (*.wav)")
    if file_path:
        global_vars.ui.audioPathEdit.setText(file_path[0])

def update_wordlist() -> None:
    """Update the wordlist."""
    new_wordlist = load_wordlist()
    model = completer.model()
    if not isinstance(model, QStringListModel):
        completer.setModel(QStringListModel(new_wordlist))
    else:
        model.setStringList(new_wordlist)

def check_for_updates():
    """
    Check for a file called MultipackParser under /media/ and /mnt/.
    If it exists, spawn an updater process to replace the current binary.
    """
    # TODO: Add visual feedback to the user so that they know that the application is checking for updates
    # disable the main window for interaction
    main_window.setDisabled(True)
    msg_box = QMessageBox()
    msg_box.setWindowTitle("Update suchen")
    msg_box.setText("Es wird nach einem Update gesucht.<br>Bitte warten Sie, während der Update-Prozess ausgeführt wird.")
    msg_box.setIcon(QMessageBox.Icon.Information)
    msg_box.show()
    
    search_paths = ["/media", "/mnt"]
    update_file_name = "MultipackParser"
    found_update_file = None

    # Traverse /media and /mnt to find the update file
    for base_path in search_paths:
        for root, dirs, files in os.walk(base_path):
            if update_file_name in files:
                found_update_file = os.path.join(root, update_file_name)
                break
        if found_update_file:
            break

    if not found_update_file:
        logger.info("No update file found.")
        # enable the main window for interaction
        main_window.setDisabled(False)
        # show a message box to the user that no update file was found
        msg_box.setWindowTitle("Keine Updates gefunden")
        msg_box.setText("Es wurden keine Updates gefunden. Die aktuelle Version ist die neueste Version.")
        msg_box.exec()
        return

    logger.info(f"Update file found: {found_update_file}")
    # show a message box to the user that an update was found
    msg_box.setWindowTitle("Update gefunden")
    msg_box.setText("Es wurde ein Update gefunden.<br>Bitte warten Sie, während der Update-Prozess ausgeführt wird.")

    # copy the new binary to cwd/update/MultipackParser and make it executable
    os.makedirs("update", exist_ok=True)
    shutil.copy(found_update_file, "update/MultipackParser")
    os.chmod("update/MultipackParser", 0o755)
    found_update_file = f"{os.getcwd()}/update/MultipackParser"
    logger.debug("New binary copied to update/MultipackParser")
    logger.debug("New binary is executable")
    logger.debug("Checking for new Version")
    
    # run the new binary with --version to check for version string and compare it to the global_arg.VERSION
    new_version = subprocess.check_output([f"{found_update_file}", "--version"]).decode().strip()
    # the version string is in the format "Multipack Parser Application Version: 1.5.3-beta"
    new_version = new_version.split(" ")[-1]
    logger.debug(f"New version: {new_version}")
    
    current_version = global_vars.VERSION.split("-")
    current_version_tag = current_version[1] if len(current_version) > 1 else None
    current_version = current_version[0].split(".")
    
    new_version = new_version.split("-")
    new_version_tag = new_version[1] if len(new_version) > 1 else None
    new_version = new_version[0].split(".")
    
    # Convert str[] to int[]
    current_version = [int(x) for x in current_version]
    new_version = [int(x) for x in new_version]

    # Compare versions
    if new_version[0] <= current_version[0] and new_version[1] <= current_version[1] and new_version[2] <= current_version[2]:
        logger.info("No new version found.")
        # change the text of the message box to "Die aktuelle Version ist die neuere Version."
        msg_box.setText("Die aktuelle Version ist die neuere Version.")
        return
    
    logger.info(f"New version found: {new_version}")
    # change the text of the message box to "Update gefunden. Version {new_version} ist verfügbar.<br>Bitte warten Sie, während der Update-Prozess ausgeführt wird."
    msg_box.setText(f"Update gefunden. Version {new_version} wird installiert.<br>Bitte warten Sie, während der Update-Prozess ausgeführt wird.")
    # Spawn an updater process
    current_binary = sys.argv[0]  # Path to the running binary
    updater_script = f"""#!/bin/bash
# Wait for the parent process to terminate
sleep 5

# Replace the current binary with the update file
cp "{found_update_file}" "{current_binary}"
chmod +x "{current_binary}"  # Ensure the binary is executable

rm -rf "{os.getcwd()}/update"

# Optionally reboot the system immediately
reboot
"""

    # Write the updater script to a temporary file
    updater_path = f"{os.getcwd()}/update/updater_script.sh"
    with open(updater_path, "w") as f:
        f.write(updater_script)

    # Make the script executable
    os.chmod(updater_path, 0o755)  # Ensure the script is executable

    # Spawn the updater process
    subprocess.Popen(  # Spawn the updater process
        ["/bin/bash", updater_path],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        close_fds=True,
        start_new_session=True,  # Detach the process
    )

    logger.info("Updater process spawned. Exiting current application.")  # Log the process spawn
    exit_app()

def spawn_play_stepback_warning_thread():
    """
    Spawn a thread to play the stepback warning.
    """
    global audio_thread, audio_thread_running
    if audio_thread is None:
        audio_thread_running = True
        audio_thread = threading.Thread(target=play_stepback_warning)
        audio_thread.daemon = True
        audio_thread.start()

def kill_play_stepback_warning_thread():
    """
    Kill the thread playing the stepback warning.
    """
    global audio_thread, audio_thread_running
    audio_thread_running = False
    if audio_thread:
        audio_thread = None

def play_stepback_warning():
    """
    Play the stepback warning in a loop using aplay.
    """
    global audio_thread_running
    
    try:
        while audio_thread_running:
            try:
                # Use aplay to play the audio file
                subprocess.run(['aplay', settings.settings['admin']['alarm_sound_file']], 
                                check=True,
                                stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL)
                logger.debug("Stepback warning played")
                time.sleep(0.1)  # Small delay between loops
                
            except subprocess.CalledProcessError as e:
                logger.error(f"Error during playback: {e}")
                break
                
    except Exception as e:
        logger.error(f"Error in audio thread: {e}")
    finally:
        logger.debug("Audio thread stopping")

def set_audio_volume() -> None:
    """Set system audio volume using amixer"""
    if not global_vars.ui:
        logger.error("UI not initialized")
        return
        
    if not global_vars.audio_muted:
        volume = '0%'
        icon_name = ":/Sound/imgs/volume-off.png"
    else:
        volume = '100%'
        icon_name = ":/Sound/imgs/volume-on.png"
    logger.debug(f"Setting audio volume to {volume}")
    try:
        subprocess.run(['amixer', 'set', 'Master', volume], 
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL,
                        check=True)
        global_vars.ui.pushButtonVolumeOnOff.setIcon(QIcon(icon_name))
        global_vars.audio_muted = not global_vars.audio_muted
    except Exception as e:
        logger.error(f"Error setting volume: {e}")

def delay_warning_sound():
    """
    This should be called in a thread at the start of the application. and never get stopped.
    Delays the warning sound start by 40 seconds.
    The sound starts if global_vars.timestamp_scanner_fault is not None and 40 seconds or older than current time.
    """
    while True:
        if global_vars.timestamp_scanner_fault and (datetime.now() - global_vars.timestamp_scanner_fault).total_seconds() >= 40:
            if not audio_thread_running:
                spawn_play_stepback_warning_thread()
        if global_vars.timestamp_scanner_fault is None:
            kill_play_stepback_warning_thread()
        time.sleep(5)

#################
# Main function #
#################

class CustomDoubleValidator(QDoubleValidator):
    """Custom double validator that allows commas to be used as decimal separators."""
    
    def validate(self, arg__1: str, arg__2: int) -> object:
        """Validate the input."""
        return super().validate(arg__1.replace(',', '.'), arg__2)

    def fixup(self, input: str) -> str:
        """Fixup the input."""
        return input.replace(',', '.')

# Main function to run the application
def main():
    """
    Main function to run the application.
    """
    global main_window
    parser = argparse.ArgumentParser(description="Multipack Parser Application")
    parser.add_argument('--version', action='store_true', help='Show version information and exit')
    parser.add_argument('-v', '--verbose', action='store_true', help='Enable verbose logging')
    parser.add_argument('--rob-path', type=str, help='Path to the .rob files')
    args = parser.parse_args()

    if args.version:
        print(f"Multipack Parser Application Version: {global_vars.VERSION}")
        return
    if args.verbose:
        # enable verbose logging
        logger.setLevel(logging.DEBUG)
    if args.rob_path:
        # try to check if the path is valid
        if os.path.exists(args.rob_path):
            # set the path to the .rob files
            global_vars.PATH_USB_STICK = args.rob_path
        else:
            logger.error(f"Path {args.rob_path} does not exist")
            return
        
    logger.debug(f"MultipackParser Application Version: {global_vars.VERSION}")

    # Update locale setting
    QLocale.setDefault(QLocale(QLocale.Language.German, QLocale.Country.Germany))

    app = QApplication(sys.argv)
    main_window = QMainWindow()
    main_window.setWindowFlags(Qt.WindowType.FramelessWindowHint)  # Use WindowType enum
    global_vars.ui = Ui_Form()
    global_vars.ui.setupUi(main_window)
    global_vars.ui.stackedWidget.setCurrentIndex(0)
    global_vars.ui.tabWidget_2.setCurrentIndex(0)

    # init settings
    init_settings()
    
    # set last restart and number of use cycles
    settings.settings['info']['last_restart'] = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    settings.settings['info']['number_of_use_cycles'] = str(int(settings.settings['info']['number_of_use_cycles']) + 1)
    settings.save_settings()

    logger.debug(f"{sys.argv=}")
    logger.debug(f"{global_vars.VERSION=}")
    logger.debug(f"{global_vars.PATH_USB_STICK=}")

    # Set the regular expression validator for EingabePallettenplan
    regex = QRegularExpression(r"^[0-9\-_]*$")
    validator = QRegularExpressionValidator(regex)
    global_vars.ui.EingabePallettenplan.setValidator(validator)

    set_wordlist()

    # Apply QIntValidator to restrict the input to only integers
    int_validator = QIntValidator()
    global_vars.ui.EingabeKartonhoehe.setValidator(int_validator)

    # Apply CustomDoubleValidator to restrict the input to only numbers
    float_validator = CustomDoubleValidator()
    float_validator.setNotation(QDoubleValidator.Notation.StandardNotation)
    float_validator.setDecimals(2)  # Set to desired number of decimals
    global_vars.ui.EingabeKartonGewicht.setValidator(float_validator)

    # if the user entered a Artikelnummer in the text box and presses enter it calls the load function
    global_vars.ui.EingabePallettenplan.returnPressed.connect(load)

    #Page 1 Buttons
    global_vars.ui.ButtonSettings.clicked.connect(open_password_dialog)
    global_vars.ui.LadePallettenplan.clicked.connect(load)
    global_vars.ui.ButtonOpenParameterRoboter.clicked.connect(lambda: open_page(Page.PARAMETER_PAGE))
    global_vars.ui.ButtonDatenSenden.clicked.connect(send_data)
    global_vars.ui.startaudio.clicked.connect(spawn_play_stepback_warning_thread)
    global_vars.ui.stopaudio.clicked.connect(kill_play_stepback_warning_thread)
    # when global_vars.ui.pushButtonVolumeOnOff is clicked and changed to state checked then set the audio volume of the system to 0% if it is not checked then set it to 100%
    global_vars.ui.pushButtonVolumeOnOff.clicked.connect(set_audio_volume)

    #Page 2 Buttons
    # Roboter Tab
    global_vars.ui.ButtonZurueck.clicked.connect(lambda: open_page(Page.MAIN_PAGE))
    global_vars.ui.ButtonRoboterStart.clicked.connect(send_cmd_play)
    global_vars.ui.ButtonRoboterPause.clicked.connect(send_cmd_pause)
    global_vars.ui.ButtonRoboterStop.clicked.connect(send_cmd_stop)
    global_vars.ui.ButtonStopRPCServer.clicked.connect(server_stop)
    # Aufnahme Tab
    global_vars.ui.ButtonZurueck_2.clicked.connect(lambda: open_page(Page.MAIN_PAGE))
    global_vars.ui.ButtonDatenSenden_2.clicked.connect(send_data)

    # Page 3 Stuff
    # Settings Tab
    global_vars.ui.ButtonZurueck_3.clicked.connect(leave_settings_page)
    global_vars.ui.pushButtonSpeichern.clicked.connect(settings.save_settings)
    global_vars.ui.lineEditDisplayHeight.textChanged.connect(lambda text: settings.settings['display']['specs'].__setitem__('height', int(text)))
    global_vars.ui.lineEditDisplayWidth.textChanged.connect(lambda text: settings.settings['display']['specs'].__setitem__('width', int(text)))
    global_vars.ui.lineEditDisplayRefreshRate.textChanged.connect(lambda text: settings.settings['display']['specs'].__setitem__('refresh_rate', int(text)))
    global_vars.ui.lineEditDisplayModel.textChanged.connect(lambda text: settings.settings['display']['specs'].__setitem__('model', text))
    global_vars.ui.comboBoxChooseURModel.currentTextChanged.connect(lambda text: settings.settings['info'].__setitem__('UR_Model', text))
    global_vars.ui.lineEditURSerialNo.textChanged.connect(lambda text: settings.settings['info'].__setitem__('UR_Serial_Number', text))
    global_vars.ui.lineEditURManufacturingDate.textChanged.connect(lambda text: settings.settings['info'].__setitem__('UR_Manufacturing_Date', text))
    global_vars.ui.lineEditURSoftwareVer.textChanged.connect(lambda text: settings.settings['info'].__setitem__('UR_Software_Version', text))
    global_vars.ui.lineEditURName.textChanged.connect(lambda text: settings.settings['info'].__setitem__('Pallettierer_Name', text))
    global_vars.ui.lineEditURStandort.textChanged.connect(lambda text: settings.settings['info'].__setitem__('Pallettierer_Standort', text))
    global_vars.ui.lineEditNumberPlans.textChanged.connect(lambda text: settings.settings['info'].__setitem__('number_of_plans', int(text)))
    global_vars.ui.lineEditNumberCycles.textChanged.connect(lambda text: settings.settings['info'].__setitem__('number_of_use_cycles', int(text)))
    global_vars.ui.lineEditLastRestart.textChanged.connect(lambda text: settings.settings['info'].__setitem__('last_restart', text))
    global_vars.ui.pathEdit.textChanged.connect(lambda text: settings.settings['admin'].__setitem__('path', text))
    global_vars.ui.buttonSelectRobPath.clicked.connect(open_folder_dialog)
    global_vars.ui.audioPathEdit.textChanged.connect(lambda text: settings.settings['admin'].__setitem__('alarm_sound_file', text))
    global_vars.ui.buttonSelectAudioFilePath.clicked.connect(open_file_dialog)
    set_settings_line_edits()
    #global_vars.ui.checkBox.stateChanged.connect(lambda state: settings.settings['audio'].__setitem__('sound', state == Qt.Checked))
    #global_vars.ui.checkBox.setChecked(settings.settings['audio']['sound'])
    #
    # TODO: implement the rest of the settings
    global_vars.ui.ButtonZurueck_4.clicked.connect(leave_settings_page)
    global_vars.ui.pushButtonSpeichern_2.clicked.connect(settings.save_settings)

    def hash_password(password, salt=None):
        """
        Hash a password.

        Args:
            password (str): The password to be hashed.
            salt (str, optional): The salt to be used. Defaults to None.

        Returns:
            str: The hashed password.
        """
        if salt is None:
            salt = os.urandom(16)
        salted_password = salt + password.encode()
        hashed_password = hashlib.sha256(salted_password).hexdigest()
        return salt.hex() + '$' + hashed_password

    global_vars.ui.passwordEdit.textChanged.connect(lambda text: settings.settings['admin'].__setitem__('password', hash_password(text)) if text else None)
    #
    global_vars.ui.ButtonZurueck_5.clicked.connect(leave_settings_page)
    global_vars.ui.pushButtonSpeichern_3.clicked.connect(save_open_file)
    global_vars.ui.pushButtonOpenFile.clicked.connect(open_file)

    global_vars.ui.ButtonZurueck_6.clicked.connect(leave_settings_page)
    global_vars.ui.pushButtonSpeichern_4.clicked.connect(settings.save_settings)

    global_vars.ui.ButtonZurueck_7.clicked.connect(leave_settings_page)
    global_vars.ui.lineEditCommand.setText("> ")  # Set initial text
    global_vars.ui.lineEditCommand.setPlaceholderText("command")  # Set placeholder text
    global_vars.ui.lineEditCommand.returnPressed.connect(execute_command)

    global_vars.ui.pushButtonSearchUpdate.clicked.connect(check_for_updates)
    global_vars.ui.pushButtonExitApp.clicked.connect(restart_app)


    # TODO: Remove this closing key combination once out of development
    ###########################################################
    global allow_close
    allow_close = False

    def allow_close_event(event):
        """
        Allow the window to close.

        Args:
            event (QEvent): The event object.
        """
        global allow_close
        if allow_close:
            event.accept()
            allow_close = False
        else:
            event.ignore()

    def handle_key_press_event(event) -> None:
        """Handle key press events."""
        global allow_close
        if (event.modifiers() == (Qt.KeyboardModifier.ControlModifier | 
                                 Qt.KeyboardModifier.AltModifier | 
                                 Qt.KeyboardModifier.ShiftModifier) and 
            event.key() == Qt.Key.Key_C):
            allow_close = True
            exit_app()
        elif (event.modifiers() == (Qt.KeyboardModifier.ControlModifier | 
                                   Qt.KeyboardModifier.AltModifier) and 
              event.key() == Qt.Key.Key_N):
            messageBox = QMessageBox()
            messageBox.setWindowTitle("Multipack Parser")
            messageBox.setTextFormat(Qt.TextFormat.RichText)
            messageBox.setText('''
            <div style="text-align: center;">
            Yann-Luca Näher - \u00a9 2024<br>
            <a href="https://github.com/Snupai">Github</a>
            </div>''')
            messageBox.setStandardButtons(QMessageBox.StandardButton.Ok)
            messageBox.setDefaultButton(QMessageBox.StandardButton.Ok)
            messageBox.exec()
            main_window.setWindowState(main_window.windowState() ^ Qt.WindowState.WindowActive)  # This will make the window blink
            
    main_window.closeEvent = allow_close_event
    main_window.keyPressEvent = handle_key_press_event
    ###########################################################
    main_window.show()
    sys.exit(app.exec())

if __name__ == "__main__":
    main()
