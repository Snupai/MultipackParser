#    Multipack Parser Application - to parse the data from the Multipack Robot to an UR Robot
#    Copyright (C) 2024  Yann-Luca Näher
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <https://www.gnu.org/licenses/>.


#TODO: Implement option for UR10e or UR20 robot. If UR20 is selected robot will have 2 pallets. else only it is like the old code.
#TODO: Implement seemless palletizing with 2 pallets for UR20 robot.
#TODO: Implement option for selecting a path for the palletizing plans. use a folder dialog for selecting the path.

import sys
import subprocess
import argparse
import hashlib
import os
import shutil
import time
import threading
import socket
import xmlrpc.client
from xmlrpc.server import SimpleXMLRPCServer
from pydub import AudioSegment
from pydub.playback import play
import logging
from datetime import datetime

# import the needed qml modules for the virtual keyboard to work
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtQuick import QQuickView
################################################################
from PySide6.QtWidgets import QApplication, QMainWindow, QDialog, QMessageBox, QWidget, QCompleter, QLineEdit, QFileDialog, QMessageBox
from PySide6.QtCore import Qt, QEvent, QFileSystemWatcher, QUrl, QProcess, QRegularExpression, QLocale
from PySide6.QtGui import QIntValidator, QDoubleValidator, QPixmap, QRegularExpressionValidator
from ui_files.ui_main_window import Ui_Form
from ui_files import MainWindowResources_rc
from utils import global_vars
from utils.settings import Settings
from utils import UR_Common_functions as UR
from utils import UR10_Server_functions as UR10
from utils import UR20_Server_functions as UR20

logger = global_vars.logger

os.environ["QT_IM_MODULE"] = "qtvirtualkeyboard"

# global_vars.PATH_USB_STICK = 'E:\' # Pfad zu den .rob Dateien nur auskommentieren zum testen
 
if global_vars.PATH_USB_STICK == '..':
    # Bekomme CWD und setze den Pfad auf den Überordner
    logger.debug(os.path.dirname(os.getcwd()))
    global_vars.PATH_USB_STICK = f'{os.path.dirname(os.getcwd())}/' 

# TODO: Rename the Einzelpaket checkbox to Einzelpaket längs greifen.
####################
# Server functions #
####################
                 
def server_start():
    """
    Start the XMLRPC server.

    Returns:
        0
    """

    global server
    server = SimpleXMLRPCServer(("", 8080), allow_none=True)
    logger.debug("Start Server")
    
    def set_robot_type(robot_type):
        """
        Set the robot type and register appropriate functions.
        
        Args:
            robot_type (str): Either 'UR10' or 'UR20'
            
        Returns:
            bool: True if robot type was set successfully
        """
        logger.debug(f"Setting robot type to: {robot_type}")
        
        if robot_type not in ['UR10', 'UR20']:
            logger.error(f"Invalid robot type: {robot_type}")
            return False
            
        # Register common functions for both robot types
        server.register_function(UR.UR_SetFileName, "UR_SetFileName")
        server.register_function(UR.UR_ReadDataFromUsbStick, "UR_ReadDataFromUsbStick")
        server.register_function(UR.UR_Palette, "UR_Palette")
        server.register_function(UR.UR_Karton, "UR_Karton")
        server.register_function(UR.UR_Lagen, "UR_Lagen")
        server.register_function(UR.UR_Zwischenlagen, "UR_Zwischenlagen")
        server.register_function(UR.UR_PaketPos, "UR_PaketPos")
        server.register_function(UR.UR_AnzLagen, "UR_AnzLagen")
        server.register_function(UR.UR_PaketeZuordnung, "UR_PaketeZuordnung")
        server.register_function(UR.UR_Paket_hoehe, "UR_Paket_hoehe")
        server.register_function(UR.UR_Startlage, "UR_Startlage")
        server.register_function(UR.UR_Quergreifen, "UR_Quergreifen")
        server.register_function(UR.UR_CoG, "UR_CoG")
        server.register_function(UR.UR_MasseGeschaetzt, "UR_MasseGeschaetzt")
        server.register_function(UR.UR_PickOffsetX, "UR_PickOffsetX")
        server.register_function(UR.UR_PickOffsetY, "UR_PickOffsetY")
        server.register_function(UR.UR_StepBack, "UR_StepBack")
        
        
        # Register robot type specific functions here if needed
        if robot_type == 'UR10':
            server.register_function(UR10.UR10_scanner1and2niobild, "UR_scanner1and2niobild")
            server.register_function(UR10.UR10_scanner1bild, "UR_scanner1bild")
            server.register_function(UR10.UR10_scanner2bild, "UR_scanner2bild")
            server.register_function(UR10.UR10_scanner1and2iobild, "UR_scanner1and2iobild")
        elif robot_type == 'UR20':
            server.register_function(UR20.UR20_SetActivePalette, "UR_SetActivePalette")
            server.register_function(UR20.UR20_GetActivePaletteNumber, "UR_GetActivePaletteNumber")
            server.register_function(UR20.UR20_GetPaletteStatus, "UR_GetPaletteStatus")
            server.register_function(UR20.UR20_scannerStatus, "UR_scannerStatus")
            
        logger.debug(f"Successfully registered functions for {robot_type}")
        return True
    
    # Register the set_robot_type function
    server.register_function(set_robot_type, "set_robot_type")
    
    server.serve_forever()
    return 0
 
def server_stop():
    """
    Stop the XMLRPC server.
    """
    server.shutdown()
 
def server_thread():
    """
    Start the XMLRPC server in a separate thread.
    """
    xServerThread = threading.Thread(target=server_start)
    xServerThread.start()
 
def send_cmd_play():
    """
    Send a command to the robot to start.
    """
    try:
        # Create a TCP/IP socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        
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
 
def send_cmd_pause():
    """
    Send a command to the robot to pause.
    """
    try:
        # Create a TCP/IP socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        
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
 
def send_cmd_stop():
    """
    Send a command to the robot to stop.
    """
    try:
        # Create a TCP/IP socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        
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

def open_password_dialog() -> None:
    """
    Open the password dialog.
    """
    from ui_files.PasswordDialog import PasswordEntryDialog  # Import here instead of top
    dialog = PasswordEntryDialog(parent_window=main_window)
    #dialog.setModal(False) #cant set to true because it will block the qtvirtualkeyboard
    dialog.show()
    dialog.ui.lineEdit.setFocus()
    dialog.exec()
    if dialog.password_accepted:
        open_settings_page()

def open_settings_page() -> None:
    """
    Open the settings page.
    """
    # set page of the stacked widgets to index 2
    settings.reset_unsaved_changes()
    global_vars.ui.stackedWidget.setCurrentIndex(2)

def open_parameter_page() -> None:
    """
    Open the parameter page.
    """
    # set page of the stacked widgets to index 1
    global_vars.ui.tabWidget.setCurrentIndex(0)
    global_vars.ui.stackedWidget.setCurrentIndex(1)

def open_main_page() -> None:
    """
    Open the main page.
    """
    # set page of the stacked widgets to index 0
    global_vars.ui.stackedWidget.setCurrentIndex(0)

def open_explorer() -> None:
    """
    Open the explorer.
    """
    logger.info("Opening explorer")
    if sys.platform == "win32":
        subprocess.Popen(["explorer.exe"])
    elif sys.platform == "linux":
        subprocess.Popen(["xdg-open", "."])

def open_terminal() -> None:
    """
    Open the terminal.
    """
    logger.info("Opening terminal")
    if sys.platform == "win32":
        subprocess.Popen(["start", "cmd.exe"], shell=True)
    elif sys.platform == "linux":
        subprocess.Popen(["x-terminal-emulator", "--window"])

def load() -> None:
    """
    Load the selected file.

    This function is called when the user clicks the "Lade Palettenplan" button.
    """
    # get the value of the EingabePalettenplan text box and run UR_SET_FILENAME then check if the file exists and if it doesnt open a message box
    Artikelnummer = global_vars.ui.EingabePallettenplan.text()
    UR.UR_SetFileName(Artikelnummer)
    
    errorReadDataFromUsbStick = UR.UR_ReadDataFromUsbStick()

    if errorReadDataFromUsbStick == 1:
        logger.error(f"Error reading file for {Artikelnummer=} no file found")
        global_vars.ui.LabelPalletenplanInfo.setText("Kein Plan gefunden")
        global_vars.ui.LabelPalletenplanInfo.setStyleSheet("color: red")
    else:
        # remove the editing focus from the text box
        global_vars.ui.EingabePallettenplan.clearFocus()
        logger.debug(f"File for {Artikelnummer=} found")
        global_vars.ui.LabelPalletenplanInfo.setText("Plan erfolgreich geladen")
        global_vars.ui.LabelPalletenplanInfo.setStyleSheet("color: green")
        global_vars.ui.ButtonOpenParameterRoboter.setEnabled(True)
        global_vars.ui.ButtonDatenSenden.setEnabled(True)
        global_vars.ui.EingabeKartonGewicht.setEnabled(True)
        global_vars.ui.EingabeKartonhoehe.setEnabled(True)
        global_vars.ui.EingabeStartlage.setEnabled(True)
        global_vars.ui.checkBoxEinzelpaket.setEnabled(True)

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
    """
    Initialize the settings.
    
    This function is called when the application starts.
    """
    global settings
    settings = Settings()
    global_vars.PATH_USB_STICK = settings.settings['admin']['path']
    logger.debug(f"Settings: {settings}")

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
        response = QMessageBox.question(main_window, "Verwerfen oder Speichern", "Möchten Sie die neuen Daten verwerfen oder speichern?",
                                        QMessageBox.Discard | QMessageBox.Save, QMessageBox.Save)
        main_window.setWindowState(main_window.windowState() ^ Qt.WindowActive)  # This will make the window blink
        if response == QMessageBox.Save:
            try:
                settings.save_settings()
                logger.debug("New settings saved.")
            except Exception as e:
                logger.error(f"Failed to save settings: {e}")
                QMessageBox.critical(main_window, "Error", f"Failed to save settings: {e}")
                return
        elif response == QMessageBox.Discard:
            settings.reset_unsaved_changes()
            set_settings_line_edits()
            logger.debug("All changes discarded.")
    
    # Navigate back to the main page
    open_main_page()

def open_file():
    """
    Open a file.

    This function is called when the user clicks the "Open" button in the editor settings tab.
    """
    # Open a file browser to select a file
    file_path, _ = QFileDialog.getOpenFileName(parent=main_window, caption="Open File")
    global_vars.ui.lineEditFilePath.setText(file_path)
    logger.debug(f"File path: {global_vars.ui.lineEditFilePath.text()}")
    
    # Open the selected file and load its content into the text edit widget
    try:
        with open(file_path, 'r') as file:
            file_content = file.read()
        global_vars.ui.textEditFile.setPlainText(file_content)  # Use setPlainText for QTextEdit
    except Exception as e:
        logger.error(f"Failed to open file: {e}")
        QMessageBox.critical(main_window, "Error", f"Failed to open file: {e}")
        main_window.setWindowState(main_window.windowState() ^ Qt.WindowActive)  # This will make the window blink

def save_open_file():
    """
    Save or open a file.

    This function is called when the user clicks the "Speichern" button in the editor settings tab.
    """
    # save the file to the selected file path but prompt the user before overwriting the file
    file_path = global_vars.ui.lineEditFilePath.text()
    if file_path:
        if os.path.exists(file_path):
            overwrite = QMessageBox.question(main_window, "Overwrite File?", f"The file {file_path} already exists. Do you want to overwrite it?", QMessageBox.Yes | QMessageBox.No)
            main_window.setWindowState(main_window.windowState() ^ Qt.WindowActive)  # This will make the window blink
            if overwrite == QMessageBox.Yes:
                with open(file_path, 'w') as file:
                    file.write(global_vars.ui.textEditFile.toPlainText())
            else:
                logger.debug("File not saved.")
        else:
            with open(file_path, 'w') as file:
                file.write(global_vars.ui.textEditFile.toPlainText())
    else:
        logger.debug("File not saved.")
        QMessageBox.warning(main_window, "Error", "Please select a file to save.")

def execute_command():
    """
    Execute a command in the console.
    """
    command = global_vars.ui.lineEditCommand.text().strip()

    # Check if the command starts with ">"
    if command.startswith("> "):
        command = command[2:].strip()  # Remove the "> " prefix

    if not command:
        return

    # Clear the console if the command is 'cls'
    if command.lower() == 'cls':
        global_vars.ui.textEditConsole.clear()
        global_vars.ui.lineEditCommand.setText("> ")  # Reset lineEdit with the prefix
        return

    global_vars.ui.textEditConsole.append(f"$ {command}")
    global_vars.ui.lineEditCommand.setText("> ")  # Reset lineEdit with the prefix

    process = QProcess()
    process.setProcessChannelMode(QProcess.MergedChannels)
    process.readyReadStandardOutput.connect(handle_stdout)
    process.readyReadStandardError.connect(handle_stderr)

    process.start(command)

    global_vars.process = process

def handle_stdout():
    """
    Handle standard output.
    """
    data = global_vars.process.readAllStandardOutput()
    stdout = bytes(data).decode("utf-8", errors="replace")
    global_vars.ui.textEditConsole.append(stdout)

def handle_stderr():
    """
    Handle standard error output.
    """
    data = global_vars.process.readAllStandardError()
    stderr = bytes(data).decode("utf-8", errors="replace")
    global_vars.ui.textEditConsole.append(stderr)

def set_settings_line_edits():
    """
    Set the line edits in the settings page to the current settings.

    This function is called when the settings page is opened or when the settings are changed.
    """
    global_vars.ui.lineEditDisplayHeight.setText(str(settings.settings['display']['specs']['height']))
    global_vars.ui.lineEditDisplayWidth.setText(str(settings.settings['display']['specs']['width']))
    global_vars.ui.lineEditDisplayRefreshRate.setText(str(int(float(settings.settings['display']['specs']['refresh_rate']))))
    global_vars.ui.lineEditDisplayModel.setText(settings.settings['display']['specs']['model'])
    global_vars.ui.lineEditURModel.setText(settings.settings['info']['UR_Model'])
    global_vars.ui.lineEditURSerialNo.setText(settings.settings['info']['UR_Serial_Number'])
    global_vars.ui.lineEditURManufacturingDate.setText(settings.settings['info']['UR_Manufacturing_Date'])
    global_vars.ui.lineEditURSoftwareVer.setText(settings.settings['info']['UR_Software_Version'])
    global_vars.ui.lineEditURName.setText(settings.settings['info']['Pallettierer_Name'])
    global_vars.ui.lineEditURStandort.setText(settings.settings['info']['Pallettierer_Standort'])
    global_vars.ui.lineEditNumberPlans.setText(str(settings.settings['info']['number_of_plans']))
    global_vars.ui.lineEditNumberCycles.setText(str(settings.settings['info']['number_of_use_cycles']))
    global_vars.ui.lineEditLastRestart.setText(settings.settings['info']['last_restart'])
    global_vars.ui.pathEdit.setText(settings.settings['admin']['path'])

def exit_app():
    """
    Restart the application.
    
    This function is called when the user clicks the "Exit Application" button.
    It spawns a new process and exits the current one.
    """
    try:
        settings.compare_loaded_settings_to_saved_settings()
        # Spawn new process and exit current one
        QProcess.startDetached(sys.executable, [sys.argv[0]])
        sys.exit(0)
    except ValueError as e:
        logger.error(f"Error: {e}")
    
        # If settings do not match, ask whether to discard or save the new data
        response = QMessageBox.question(main_window, "Verwerfen oder Speichern", "Möchten Sie die neuen Daten verwerfen oder speichern?",
                                        QMessageBox.Discard | QMessageBox.Save, QMessageBox.Save)
        main_window.setWindowState(main_window.windowState() ^ Qt.WindowActive)  # This will make the window blink
        if response == QMessageBox.Save:
            try:
                settings.save_settings()
                logger.debug("New settings saved.")
                # Spawn new process and exit current one
                QProcess.startDetached(sys.executable, [sys.argv[0]])
                sys.exit(0)
            except Exception as e:
                logger.error(f"Failed to save settings: {e}")
                QMessageBox.critical(main_window, "Error", f"Failed to save settings: {e}")
                return
        elif response == QMessageBox.Discard:
            settings.reset_unsaved_changes()
            set_settings_line_edits()
            logger.debug("All changes discarded.")


def set_wordlist():
    """
    Set the wordlist.
    """
    global completer  # Declare completer as global
    wordlist = load_wordlist()
    completer = QCompleter(wordlist, main_window)  # Now this will access the global variable
    global_vars.ui.EingabePallettenplan.setCompleter(completer)

    file_watcher = QFileSystemWatcher([global_vars.PATH_USB_STICK], main_window)
    file_watcher.directoryChanged.connect(update_wordlist)

def open_folder_dialog():
    """
    Open the folder dialog.
    """
    # show warning dialog if the user wants to set the path
    # only if the user acknowledges the warning dialog and the risks then continue with choosing the folder else cancel asap
    response = QMessageBox.warning(main_window, "Warnung! - Mögliche Risiken!", "<b>Möchten Sie den Pfad wirklich ändern?</b><br>Dies könnte zu Problemen führen, wenn bereits ein Palettenplan geladen ist und nach dem Setzen des Pfades nicht ein neuer geladen wird.", QMessageBox.Yes | QMessageBox.No)
    main_window.setWindowState(main_window.windowState() ^ Qt.WindowActive)  # This will make the window blink
    if response == QMessageBox.Yes:
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
        set_wordlist()  # Ensure this is defined before calling it

def update_wordlist():
    """
    Update the wordlist.
    """
    new_wordlist = load_wordlist()
    completer.model().setStringList(new_wordlist)  # This will now work with the global completer
    set_wordlist()

def check_for_updates():
    """
    Check for a file called MultipackParser under /media/ and /mnt/.
    If it exists, spawn an updater process to replace the current binary.
    """
    # TODO: Add visual feedback to the user so that they know that the application is checking for updates
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
        return

    logger.info(f"Update file found: {found_update_file}")

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
        return
    
    logger.info(f"New version found: {new_version}")

    # Spawn an updater process
    current_binary = sys.argv[0]  # Path to the running binary
    updater_script = f"""#!/bin/bash
# Wait for the parent process to terminate
sleep 5

# Replace the current binary with the update file
cp "{found_update_file}" "{current_binary}"
chmod +x "{current_binary}"  # Ensure the binary is executable

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
    sys.exit(0)  # Terminate the current process

#################
# Main function #
#################

class CustomDoubleValidator(QDoubleValidator):
    """
    Custom double validator.

    This class inherits from QDoubleValidator and overrides the validate method to allow
    commas to be used as decimal separators.
    """
    def validate(self, usr_input, pos):
        """
        Validate the input.

        Args:
            input (str): The input to be validated.
            pos (int): The position of the input in the text box.

        Returns:
            bool: True if the input is valid, False otherwise.
        """
        if ',' in usr_input:
            usr_input = usr_input.replace(',', '.')
        return super().validate(usr_input, pos)

    def fixup(self, usr_input):
        """
        Fixup the input.

        Args:
            input (str): The input to be fixed up.

        Returns:
            str: The fixed up input.
        """
        if ',' in usr_input:
            usr_input = usr_input.replace(',', '.')
        return usr_input  # Directly return the modified input without further processing

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

    QLocale.setDefault(QLocale(QLocale.German, QLocale.Germany)) # set locale to german for german keyboard layout

    app = QApplication(sys.argv)
    main_window = QMainWindow()
    main_window.setWindowFlags(Qt.FramelessWindowHint) # remove the window border to prevent moving or minimizing the window
    global_vars.ui = Ui_Form()
    global_vars.ui.setupUi(main_window)
    global_vars.ui.stackedWidget.setCurrentIndex(0)
    global_vars.ui.tabWidget_2.setCurrentIndex(0)

    init_settings()
    
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
    float_validator.setNotation(QDoubleValidator.StandardNotation)
    float_validator.setDecimals(2)  # Set to desired number of decimals
    global_vars.ui.EingabeKartonGewicht.setValidator(float_validator)

    # if the user entered a Artikelnummer in the text box and presses enter it calls the load function
    global_vars.ui.EingabePallettenplan.returnPressed.connect(load)

    #Page 1 Buttons
    global_vars.ui.ButtonSettings.clicked.connect(open_password_dialog)
    global_vars.ui.LadePallettenplan.clicked.connect(load)
    global_vars.ui.ButtonOpenParameterRoboter.clicked.connect(open_parameter_page)
    global_vars.ui.ButtonDatenSenden.clicked.connect(send_data)

    #Page 2 Buttons
    # Roboter Tab
    global_vars.ui.ButtonZurueck.clicked.connect(open_main_page)
    global_vars.ui.ButtonRoboterStart.clicked.connect(send_cmd_play)
    global_vars.ui.ButtonRoboterPause.clicked.connect(send_cmd_pause)
    global_vars.ui.ButtonRoboterStop.clicked.connect(send_cmd_stop)
    global_vars.ui.ButtonStopRPCServer.clicked.connect(server_stop)
    # Aufnahme Tab
    global_vars.ui.ButtonZurueck_2.clicked.connect(open_main_page)
    global_vars.ui.ButtonDatenSenden_2.clicked.connect(send_data)

    # Page 3 Stuff
    # Settings Tab
    global_vars.ui.ButtonZurueck_3.clicked.connect(leave_settings_page)
    global_vars.ui.pushButtonSpeichern.clicked.connect(settings.save_settings)
    global_vars.ui.lineEditDisplayHeight.textChanged.connect(lambda text: settings.settings['display']['specs'].__setitem__('height', int(text)))
    global_vars.ui.lineEditDisplayWidth.textChanged.connect(lambda text: settings.settings['display']['specs'].__setitem__('width', int(text)))
    global_vars.ui.lineEditDisplayRefreshRate.textChanged.connect(lambda text: settings.settings['display']['specs'].__setitem__('refresh_rate', int(text)))
    global_vars.ui.lineEditDisplayModel.textChanged.connect(lambda text: settings.settings['display']['specs'].__setitem__('model', text))
    global_vars.ui.lineEditURModel.textChanged.connect(lambda text: settings.settings['info'].__setitem__('UR_Model', text))
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
    global_vars.ui.pushButtonExitApp.clicked.connect(exit_app)


    # TODO: Remove this key combination once out of development
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

    def handle_key_press_event(event):
        """
        Handle key press events.

        Args:
            event (QEvent): The event object.

        Returns:
            bool: True if the event was handled, False otherwise.
        """
        global allow_close
        if (event.modifiers() == (Qt.ControlModifier | Qt.AltModifier | Qt.ShiftModifier) and 
            event.key() == Qt.Key_C):
            allow_close = True
            main_window.close()
        elif (event.modifiers() == (Qt.ControlModifier | Qt.AltModifier) and 
            event.key() == Qt.Key_N):
            messageBox = QMessageBox()
            messageBox.setWindowTitle("Multipack Parser")
            messageBox.setTextFormat(Qt.TextFormat.RichText)
            messageBox.setText('''
            <div style="text-align: center;">
            Yann-Luca Näher - \u00a9 2024<br>
            <a href="https://github.com/Snupai">Github</a>
            </div>''')
            messageBox.setStandardButtons(QMessageBox.Ok)
            messageBox.setDefaultButton(QMessageBox.Ok)
            messageBox.exec()
            main_window.setWindowState(main_window.windowState() ^ Qt.WindowActive)  # This will make the window blink
        return True

    main_window.closeEvent = allow_close_event
    main_window.keyPressEvent = handle_key_press_event
    ###########################################################
    main_window.show()
    sys.exit(app.exec())

if __name__ == "__main__":
    main()
