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


# import the needed qml modules for the virtual keyboard to work
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtQuick import QQuickView
################################################################
from PySide6.QtWidgets import QApplication, QMainWindow, QDialog, QMessageBox, QWidget, QCompleter
from PySide6.QtCore import Qt, QEvent, QFileSystemWatcher
from PySide6.QtGui import QIntValidator, QDoubleValidator, QPixmap
import sys
import logging
import subprocess
import argparse
import hashlib
import os
import time
import threading
import socket
import xmlrpc.client
from xmlrpc.server import SimpleXMLRPCServer
from pydub import AudioSegment
from pydub.playback import play
from logging.handlers import RotatingFileHandler
from ui_main_window import Ui_Form  # Import the generated main window class
from ui_password_entry import Ui_Dialog  # Import the generated dialog class
from typing import NoReturn
import MainWindowResources_rc
from utils import global_vars
from utils import UR10_Server_functions as UR10

# BUG: QtVirtualKeyboard does not work on Linux 
os.environ["QT_IM_MODULE"] = "qtvirtualkeyboard"

# global_vars.PATH_USB_STICK = 'E:\' # Pfad zu den .rob Dateien nur auskommentieren zum testen
 
if global_vars.PATH_USB_STICK == None:
    # Bekomme CWD und setze den Pfad auf den Überordner
    print(os.path.dirname(os.getcwd()))
    global_vars.PATH_USB_STICK = f'{os.path.dirname(os.getcwd())}/' 


# Setup logging
log_formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
log_handler = RotatingFileHandler('app.log', maxBytes=5*1024*1024, backupCount=2)
log_handler.setFormatter(log_formatter)
console_handler = logging.StreamHandler(sys.stdout)
console_handler.setFormatter(log_formatter)

logging.basicConfig(level=logging.INFO, handlers=[log_handler, console_handler])
global_vars.logger = logging.getLogger(__name__)

# Define the password entry dialog class
class PasswordEntryDialog(QDialog):
    password_accepted = False

    def __init__(self) -> None:
        super(PasswordEntryDialog, self).__init__()
        self.ui = Ui_Dialog()
        self.ui.setupUi(self)

        self.ui.buttonBox.accepted.connect(self.accept)
        self.ui.buttonBox.rejected.connect(self.reject)

        global_vars.logger.info("PasswordEntryDialog opened")

    def accept(self) -> NoReturn:
        entered_password = hashlib.sha256(self.ui.lineEdit.text().encode()).hexdigest()
        if self.verify_password(entered_password):
            global_vars.logger.debug("Password is correct")
            self.password_accepted = True
            super().accept()
        else:
            global_vars.logger.debug("Password is incorrect")
            QMessageBox.warning(self, "Error", "Incorrect password")

    def verify_password(self, hashed_password: str) -> bool:
        correct_password = "94edf28c6d6da38fd35d7ad53e485307f89fbeaf120485c8d17a43f323deee71"  # SHA256 hash of "password"
        # compare the hash with the correct password
        return hashed_password == correct_password

    def reject(self) -> NoReturn:
        self.password_accepted = False
        global_vars.logger.debug("rejected")
        super().reject()



####################
# Random functions #
####################
                 
def Server_start():
    #server_stop_btn.configure(state="normal")
    global server
    server = SimpleXMLRPCServer(("", 8080), allow_none=True)
    print ("Start Server")
    server.register_function(UR10.UR_SetFileName, "UR_SetFileName")
    server.register_function(UR10.UR_ReadDataFromUsbStick, "UR_ReadDataFromUsbStick")
    server.register_function(UR10.UR_Palette, "UR_Palette")
    server.register_function(UR10.UR_Karton, "UR_Karton")
    server.register_function(UR10.UR_Lagen, "UR_Lagen")
    server.register_function(UR10.UR_Zwischenlagen, "UR_Zwischenlagen")
    server.register_function(UR10.UR_PaketPos, "UR_PaketPos")
    server.register_function(UR10.UR_AnzLagen, "UR_AnzLagen")
    server.register_function(UR10.UR_AnzPakete, "UR_AnzPakete") #Veraltet - nicht mehr verwenden
    server.register_function(UR10.UR_PaketeZuordnung, "UR_PaketeZuordnung") #Picks pro Lage
    server.register_function(UR10.UR_Paket_hoehe, "UR_Paket_hoehe") #Gemessene Pakethöhe
    server.register_function(UR10.UR_Startlage, "UR_Startlage") #Startlage für Neustart
    server.register_function(UR10.UR_Quergreifen, "UR_Quergreifen") #quer oder längs greifen
    server.register_function(UR10.UR_CoG, "UR_CoG") #Rückgabe von y und z
    server.register_function(UR10.UR_MasseGeschaetzt, "UR_MasseGeschaetzt") #Schätzen des Paketgewichts mit empirischem Faktor
    server.register_function(UR10.UR_PickOffsetX, "UR_PickOffsetX")
    server.register_function(UR10.UR_PickOffsetY, "UR_PickOffsetY")#Pick-Offset zur live Korrektur
    server.register_function(UR10.UR_StepBack, "UR_StepBack") #Audiosignal für Laserscanner
    server.register_function(UR10.UR_scanner1and2niobild, "UR_scanner1and2niobild")
    server.register_function(UR10.UR_scanner1bild, "UR_scanner1bild")
    server.register_function(UR10.UR_scanner2bild, "UR_scanner2bild")
    server.register_function(UR10.UR_scanner1and2iobild, "UR_scanner1and2iobild")
    #print ("Oeffne serielle Schnittstelle")
    #ser = serial.Serial('/dev/ttyUSB0', 115200, timeout = 0.5)
    #print ("Serielle Schnittstelle " + ser.name + " 115200Baud")
    #roboter_btn.configure(state="normal")
    server.serve_forever()
    
    #print ("Server läuft")
    return 0
 
def Server_stop():
    server.shutdown()
 
def Server_thread():
    xServerThread = threading.Thread(target=Server_start)
    xServerThread.start()
 
def Send_cmd_play():
    try:
        # Create a TCP/IP socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        
        # Connect the socket to the port where the server is listening
        server_address = (global_vars.robot_ip, 29999)
        print ('connecting to %s port %s' %(server_address))
        sock.connect(server_address)
        
        # Send data
        message = 'play\n'
        print ('sending %s' %(message))
        sock.sendall(message.encode('utf-8'))
        
        # Print any response
        data = sock.recv(4096)
        print ('received %s' %(data))
        
    finally:
        print ('closing socket')
        sock.close()
 
def Send_cmd_pause():
    try:
        # Create a TCP/IP socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        
        # Connect the socket to the port where the server is listening
        server_address = (global_vars.robot_ip, 29999)
        print ('connecting to %s port %s' %(server_address))
        sock.connect(server_address)
        
        # Send data
        message = 'pause\n'
        print ('sending %s' %(message))
        sock.sendall(message.encode('utf-8'))
        
        # Print any response
        data = sock.recv(4096)
        print ('received %s' %(data))
        
    finally:
        print ('closing socket')
        sock.close()
 
def Send_cmd_stop():
    try:
        # Create a TCP/IP socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        
        # Connect the socket to the port where the server is listening
        server_address = (global_vars.robot_ip, 29999)
        print ('connecting to %s port %s' %(server_address))
        sock.connect(server_address)
        
        # Send data
        message = 'stop\n'
        print ('sending %s' %(message))
        sock.sendall(message.encode('utf-8'))
        
        # Print any response
        data = sock.recv(4096)
        print ('received %s' %(data))
        
    finally:
        print ('closing socket')
        sock.close()

################
# UI functions #
################


def open_password_dialog() -> None:
    dialog = PasswordEntryDialog()
    if dialog.exec() == QDialog.Accepted and dialog.password_accepted:
        open_settings_page()

def open_settings_page() -> None:
    # set page of the stacked widgets to index 2
        global_vars.ui.stackedWidget.setCurrentIndex(2)

def open_parameter_page() -> None:
    # set page of the stacked widgets to index 1
    global_vars.ui.tabWidget.setCurrentIndex(0)
    global_vars.ui.stackedWidget.setCurrentIndex(1)

def open_main_page() -> None:
    # set page of the stacked widgets to index 0
        global_vars.ui.stackedWidget.setCurrentIndex(0)

def open_explorer() -> None:
    global_vars.logger.info("Opening explorer")
    if sys.platform == "win32":
        subprocess.Popen(["explorer.exe"])
    elif sys.platform == "linux":
        subprocess.Popen(["xdg-open", "."])

def open_terminal() -> None:
    global_vars.logger.info("Opening terminal")
    if sys.platform == "win32":
        subprocess.Popen(["start", "cmd.exe"], shell=True)
    elif sys.platform == "linux":
        subprocess.Popen(["x-terminal-emulator", "--window"])

def load() -> None:
    # get the value of the EingabePallettenplan text box and run UR_SET_FILENAME then check if the file exists and if it doesnt open a message box
    Artikelnummer = global_vars.ui.EingabePallettenplan.text()
    UR10.UR_SetFileName(Artikelnummer)
    
    errorReadDataFromUsbStick = UR10.UR_ReadDataFromUsbStick()

    if errorReadDataFromUsbStick == 1:
        global_vars.logger.error(f"Error reading file for {Artikelnummer=} no file found")
        global_vars.ui.LabelPalletenplanInfo.setText("Kein Plan gefunden")
        global_vars.ui.LabelPalletenplanInfo.setStyleSheet("color: red")
    else:
        # remove the editing focus from the text box
        global_vars.ui.EingabePallettenplan.clearFocus()
        global_vars.logger.debug(f"File for {Artikelnummer=} found")
        global_vars.ui.LabelPalletenplanInfo.setText("Plan erfolgreich geladen")
        global_vars.ui.LabelPalletenplanInfo.setStyleSheet("color: green")
        global_vars.ui.ButtonOpenParameterRoboter.setEnabled(True)
        global_vars.ui.ButtonDatenSenden.setEnabled(True)
        global_vars.ui.EingabeKartonGewicht.setEnabled(True)
        global_vars.ui.EingabeKartonhoehe.setEnabled(True)
        global_vars.ui.EingabeStartlage.setEnabled(True)
        global_vars.ui.checkBoxEinzelpaket.setEnabled(True)

        Volumen = (global_vars.g_PaketDim[0] * global_vars.g_PaketDim[1] * global_vars.g_PaketDim[2]) / 1E+9 # in m³
        global_vars.logger.debug(f"{Volumen=}")
        Dichte = 1000 # Dichte von Wasser in kg/m³
        global_vars.logger.debug(f"{Dichte=}")
        Ausnutzung = 0.4 # Empirsch ermittelter Faktor - nicht für Gasflaschen
        global_vars.logger.debug(f"{Ausnutzung=}")
        Gewicht = round(Volumen * Dichte * Ausnutzung, 1) # Gewicht in kg
        global_vars.logger.debug(f"{Gewicht=}")
        global_vars.ui.EingabeKartonGewicht.setText(str(Gewicht))
        global_vars.ui.EingabeKartonhoehe.setText(str(global_vars.g_PaketDim[2]))

def send_data() -> None:
        Server_thread()

def load_wordlist() -> list:
    wordlist = []
    count = 0
    for file in os.listdir(global_vars.PATH_USB_STICK):
        if file.endswith(".rob"):
            wordlist.append(file[:-4])
            count = count + 1
    global_vars.logger.debug(f"{count=}")
    return wordlist



#################
# Main function #
#################

class CustomDoubleValidator(QDoubleValidator):
    def validate(self, input, pos):
        if ',' in input:
            input = input.replace(',', '.')
        return super().validate(input, pos)

    def fixup(self, input):
        if ',' in input:
            input = input.replace(',', '.')
        return input  # Directly return the modified input without further processing

# Main function to run the application
def main():
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
        global_vars.logger.setLevel(logging.DEBUG)
    if args.rob_path:
        # try to check if the path is valid
        if os.path.exists(args.rob_path):
            # set the path to the .rob files
            global_vars.PATH_USB_STICK = args.rob_path
        else:
            global_vars.logger.error(f"Path {args.rob_path} does not exist")
            return

    global_vars.logger.debug(f"{sys.argv=}")
    global_vars.logger.debug(f"{global_vars.VERSION=}")
    global_vars.logger.debug(f"{global_vars.PATH_USB_STICK=}")

    app = QApplication(sys.argv)
    main_window = QMainWindow()
    global_vars.ui = Ui_Form()
    global_vars.ui.setupUi(main_window)
    global_vars.ui.stackedWidget.setCurrentIndex(0)

    def update_wordlist():
        new_wordlist = load_wordlist()
        completer.model().setStringList(new_wordlist)

    wordlist = load_wordlist()
    completer = QCompleter(wordlist, main_window)
    global_vars.ui.EingabePallettenplan.setCompleter(completer)

    file_watcher = QFileSystemWatcher([global_vars.PATH_USB_STICK], main_window)
    file_watcher.directoryChanged.connect(update_wordlist)

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
    global_vars.ui.ButtonRoboterStart.clicked.connect(Send_cmd_play)
    global_vars.ui.ButtonRoboterPause.clicked.connect(Send_cmd_pause)
    global_vars.ui.ButtonRoboterStop.clicked.connect(Send_cmd_stop)
    global_vars.ui.ButtonStopRPCServer.clicked.connect(Server_stop)
    # Aufnahme Tab
    global_vars.ui.ButtonZurueck_2.clicked.connect(open_main_page)
    global_vars.ui.ButtonDatenSenden_2.clicked.connect(send_data)

    # Page 3 Buttons
    global_vars.ui.ButtonZurueck_3.clicked.connect(open_main_page)
    global_vars.ui.Button_OpenExplorer.clicked.connect(open_explorer)
    global_vars.ui.Button_OpenTerminal.clicked.connect(open_terminal)

    global allow_close
    allow_close = False

    def allow_close_event(event):
        global allow_close
        if allow_close:
            event.accept()
            allow_close = False
        else:
            event.ignore()

    def handle_key_press_event(event):
        global allow_close
        # Check for the key combination Ctrl + Alt + Shift + C
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
        return True

    main_window.closeEvent = allow_close_event
    main_window.keyPressEvent = handle_key_press_event
    
    main_window.show()
    sys.exit(app.exec())

if __name__ == "__main__":
    main()
