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
from PySide6.QtWidgets import QApplication, QMainWindow, QDialog, QMessageBox, QWidget, QCompleter, QLineEdit
from PySide6.QtCore import Qt, QEvent, QFileSystemWatcher
from PySide6.QtGui import QIntValidator, QDoubleValidator, QPixmap
import sys
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
import logging

from ui_files import Ui_Form, MainWindowResources_rc, PasswordEntryDialog
from utils import global_vars, Settings
from utils import UR10_Server_functions as UR10

# BUG: QtVirtualKeyboard does not work on Linux 
#os.environ["QT_IM_MODULE"] = "qtvirtualkeyboard"

# global_vars.PATH_USB_STICK = 'E:\' # Pfad zu den .rob Dateien nur auskommentieren zum testen
 
if global_vars.PATH_USB_STICK == None:
    # Bekomme CWD und setze den Pfad auf den Überordner
    print(os.path.dirname(os.getcwd()))
    global_vars.PATH_USB_STICK = f'{os.path.dirname(os.getcwd())}/' 

logger = global_vars.logger

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
    logger.info("Opening explorer")
    if sys.platform == "win32":
        subprocess.Popen(["explorer.exe"])
    elif sys.platform == "linux":
        subprocess.Popen(["xdg-open", "."])

def open_terminal() -> None:
    logger.info("Opening terminal")
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
        Server_thread()

def load_wordlist() -> list:
    wordlist = []
    count = 0
    for file in os.listdir(global_vars.PATH_USB_STICK):
        if file.endswith(".rob"):
            wordlist.append(file[:-4])
            count = count + 1
    logger.debug(f"Wordlist {count=}")
    return wordlist

def init_settings():
    global settings
    settings = Settings()
    logger.debug(f"Settings: {settings}")

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
        logger.setLevel(logging.DEBUG)
    if args.rob_path:
        # try to check if the path is valid
        if os.path.exists(args.rob_path):
            # set the path to the .rob files
            global_vars.PATH_USB_STICK = args.rob_path
        else:
            logger.error(f"Path {args.rob_path} does not exist")
            return

    logger.debug(f"{sys.argv=}")
    logger.debug(f"{global_vars.VERSION=}")
    logger.debug(f"{global_vars.PATH_USB_STICK=}")

    app = QApplication(sys.argv)
    main_window = QMainWindow()
    global_vars.ui = Ui_Form()
    global_vars.ui.setupUi(main_window)
    global_vars.ui.stackedWidget.setCurrentIndex(0)
    global_vars.ui.tabWidget_2.setCurrentIndex(0)
    
    init_settings()
    
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
    global_vars.ui.lineEditDisplayHeight.setText(str(settings.settings['display']['specs']['height']))
    global_vars.ui.lineEditDisplayWidth.setText(str(settings.settings['display']['specs']['width']))
    global_vars.ui.lineEditDisplayRefreshRate.setText(str(settings.settings['display']['specs']['refresh_rate']))
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
    #global_vars.ui.lineEditURModel.textChanged.connect(lambda text: settings.settings['info'].__setitem__('UR_Model', text))
    #global_vars.ui.checkBox.stateChanged.connect(lambda state: settings.settings['audio'].__setitem__('sound', state == Qt.Checked))
    #global_vars.ui.checkBox.setChecked(settings.settings['audio']['sound'])
    #
    # TODO: implement the rest of the settings
    global_vars.ui.ButtonZurueck_4.clicked.connect(open_main_page)
    global_vars.ui.pushButtonSpeichern_2.clicked.connect(settings.save_settings)
    global_vars.ui.passwordEdit.textChanged.connect(lambda text: settings.settings['admin'].__setitem__('password', text))
    global_vars.ui.passwordEdit.setText(settings.settings['admin']['password'])
    #
    global_vars.ui.ButtonZurueck_5.clicked.connect(open_main_page)
    global_vars.ui.pushButtonSpeichern_3.clicked.connect(settings.save_settings)
    global_vars.ui.ButtonZurueck_6.clicked.connect(open_main_page)
    global_vars.ui.pushButtonSpeichern_4.clicked.connect(settings.save_settings)

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
