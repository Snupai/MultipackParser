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


#TODO: Implement all functions properly from the old code. Maybe finished idk
#TODO: Implement option for UR10e or UR20 robot. If UR20 is selected robot will have 2 pallets. else only it is like the old code.
#TODO: Implement seemless palletizing with 2 pallets for UR20 robot.


# import the needed qml modules for the virtual keyboard to work
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtQuick import QQuickView
################################################################
from PySide6.QtWidgets import QApplication, QMainWindow, QDialog, QMessageBox, QWidget, QCompleter
from PySide6.QtCore import Qt, QEvent
from PySide6.QtGui import QIntValidator, QDoubleValidator
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

# BUG: QtVirtualKeyboard does not work on Linux 
os.environ["QT_IM_MODULE"] = "qtvirtualkeyboard"

VERSION = '1.5.0-alpha'

# IP Address of the Robot - Set to localhost only for testing
robot_ip = '192.168.0.1' # DO NOT CHANGE
 
# Konstanten
#PATH_USB_STICK     = 'E:\'
PATH_USB_STICK  = ''
 
if PATH_USB_STICK == '':
    # Bekomme CWD und setze den Pfad auf den Überordner
    PATH_USB_STICK = f'{os.path.dirname(os.getcwd())}/' 
 
# Konstanten für Datenstruktur
#List Index
LI_PALETTE_DATA = 0
LI_PACKAGE_DATA = 1
LI_LAYERTYPES = 2
LI_NUMBER_OF_LAYERS = 3
#Palette Values
LI_PALETTE_DATA_LENGTH = 0
LI_PALETTE_DATA_WIDTH = 1
LI_PALETTE_DATA_HEIGHT = 2
#Package Values
LI_PACKAGE_DATA_LENGTH = 0
LI_PACKAGE_DATA_WIDTH = 1
LI_PACKAGE_DATA_HEIGHT = 2
LI_PACKAGE_DATA_GAP = 3
#Position Values
LI_POSITION_XP = 0
LI_POSITION_YP = 1
LI_POSITION_AP = 2
LI_POSITION_XD = 3
LI_POSITION_YD = 4
LI_POSITION_AD = 5
LI_POSITION_NOP = 6
LI_POSITION_XVEC = 7
LI_POSITION_YVEC = 8
#Number of Entries
NOE_PALETTE_VALUES = 3
NOE_PACKAGE_VALUES = 4
NOE_LAYERTYPES_VALUES = 1
NOE_NUMBER_OF_LAYERS = 1
NOE_PACKAGE_PER_LAYER = 1
NOE_PACKAGE_POSITION_INFO = 9


# Setup logging
log_formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
log_handler = RotatingFileHandler('app.log', maxBytes=5*1024*1024, backupCount=2)
log_handler.setFormatter(log_formatter)
console_handler = logging.StreamHandler(sys.stdout)
console_handler.setFormatter(log_formatter)

logging.basicConfig(level=logging.INFO, handlers=[log_handler, console_handler])
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



####################
# Random functions #
####################

#Dateiname abfragen
def UR_SetFileName(Artikelnummer):
 
    global FILENAME
    
    FILENAME = (Artikelnummer + '.rob')
    logger.debug(f"{FILENAME=}")
    return FILENAME 
 
 
#daten vom usb stick hochladen und lesbar machen 
def UR_ReadDataFromUsbStick():
    global g_PalettenDim
    global g_PaketDim 
    global g_LageArten
    global g_Daten
    global g_LageZuordnung
    global g_PaketPos
    global g_AnzahlPakete
    global g_AnzLagen
    global g_PaketeZuordnung
    global g_Zwischenlagen
    global g_Startlage
    global g_paket_quer
    global g_CenterOfGravity
    global g_MassePaket
    global g_Pick_Offset_X
    global g_Pick_Offset_Y
    g_Daten = []
    g_LageZuordnung = []
    g_PaketPos = []
    g_PaketeZuordnung = []
    g_Zwischenlagen = []
    g_paket_quer = 1
    g_CenterOfGravity = [0,0,0]
    
    
    try:
        with open(PATH_USB_STICK + FILENAME) as file:
            
            for line in file:
                str = line.strip()
                tmpList = line.split('\t')
                
                for i in range(len(tmpList)):
                    tmpList[i] = int(tmpList[i])
                    
                g_Daten.append(tmpList)
 
 
            pl = g_Daten[LI_PALETTE_DATA][LI_PALETTE_DATA_LENGTH]
            pw = g_Daten[LI_PALETTE_DATA][LI_PALETTE_DATA_WIDTH]
            ph = g_Daten[LI_PALETTE_DATA][LI_PALETTE_DATA_HEIGHT]
            g_PalettenDim = [pl, pw, ph]
            
            #Kartondaten
            pl = g_Daten[LI_PACKAGE_DATA][LI_PACKAGE_DATA_LENGTH]
            pw = g_Daten[LI_PACKAGE_DATA][LI_PACKAGE_DATA_WIDTH]
            ph = g_Daten[LI_PACKAGE_DATA][LI_PACKAGE_DATA_HEIGHT]
            pr = g_Daten[LI_PACKAGE_DATA][LI_PACKAGE_DATA_GAP]
            g_PaketDim = [pl, pw, ph, pr]
            
            #Lagearten
            g_LageArten = g_Daten[LI_LAYERTYPES][0]
            
            #Lagenzuordnung
            anzLagen = g_Daten[LI_NUMBER_OF_LAYERS][0]
            g_AnzLagen = anzLagen
 
 
            index       = LI_NUMBER_OF_LAYERS + 2
            end_index   = index + anzLagen
 
 
            while index < end_index:
                
                lagenart = g_Daten[index][0]
                zwischenlagen = g_Daten[index][1]
 
                g_LageZuordnung.append(lagenart)
                g_Zwischenlagen.append(zwischenlagen)
            
                index = index +1
            
            #Paketpositionen
            ersteLage   = 4 + (anzLagen + 1)
            index       = ersteLage
            anzahlPaket = g_Daten[index][0]
            g_AnzahlPakete = anzahlPaket #Achtung veraltet - Anzahl der Picks bei Multipick
            index_paketZuordnung = index
            
            for i in range(g_LageArten):
                
                anzahlPick = g_Daten[index_paketZuordnung][0]
                g_PaketeZuordnung.append(anzahlPick)
                index_paketZuordnung = index_paketZuordnung + anzahlPick + 1
                
            
            for i in range(g_LageArten):            
                index = index + 1 #Überspringe die Zeile mit der Anzahl der Pakete
                anzahlPaket = g_PaketeZuordnung[i]
                
                for j in range(anzahlPaket):
                    xp = g_Daten[index][LI_POSITION_XP]
                    yp = g_Daten[index][LI_POSITION_YP]
                    ap = g_Daten[index][LI_POSITION_AP]
                    xd = g_Daten[index][LI_POSITION_XD]
                    yd = g_Daten[index][LI_POSITION_YD]
                    ad = g_Daten[index][LI_POSITION_AD]
                    nop = g_Daten[index][LI_POSITION_NOP]
                    xvec = g_Daten[index][LI_POSITION_XVEC]
                    yvec = g_Daten[index][LI_POSITION_YVEC]
                    packagePos = [xp, yp, ap, xd, yd, ad, nop, xvec, yvec]
                    g_PaketPos.append(packagePos)
                    index = index + 1    
 
            return 0                
    except:
        logger.error(f"Error reading file {FILENAME}")
    return 1
 
 
#funktion für den roboter 
def UR_Palette():
    return g_PalettenDim
 
def UR_Karton():
    return g_PaketDim
 
def UR_Lagen():
    return g_LageZuordnung
 
def UR_Zwischenlagen():
    return g_Zwischenlagen
 
def UR_PaketPos(Nummer):
    return g_PaketPos[Nummer]
 
def UR_AnzLagen():
    return g_AnzLagen
 
def UR_AnzPakete():
    return g_AnzahlPakete
 
def UR_PaketeZuordnung():
    return g_PaketeZuordnung
 
 
#den "center of gravity" messen
def UR_CoG(Masse_Paket,Masse_Greifer,Anzahl_Pakete=1):
 
    if(Anzahl_Pakete == 0):
        Masse_Paket=0
    #Berechnung Y
    Karton_Y = g_PaketDim[0]
    y = (1/(Masse_Greifer + Masse_Paket))*((-0.045*Masse_Greifer)+(-0.045*Masse_Paket))
    #Berechnung Z
    Karton_Z = g_PaketDim[2]
    z = (1/(Masse_Greifer + (Masse_Paket*Anzahl_Pakete)))*((0.047*Masse_Greifer)+((0.047+(Karton_Z/2000))*Masse_Paket*Anzahl_Pakete)) #Annahme Schwerpunkt Paket ist halbe Höhe u. mm zu m -> Karton_Z/2*1000
    #Zuweisung Array
    g_CenterOfGravity[0] = y
    g_CenterOfGravity[1] = z
    return g_CenterOfGravity
 
    
#die funktion für den audio file zu spielen
def UR_StepBack():
    file = AudioSegment.from_file(file = PATH_BILDER + "stepback.mp3", format = "mp3")
    play(file)
    return


def UR_Paket_hoehe():
    global ui
    g_PaketDim[2] = int(ui.EingabeKartonhoehe.text())
    return g_PaketDim[2]

def UR_Startlage():
    global ui
    g_Startlage = int(ui.EingabeStartlage.value())
    return g_Startlage

def UR_MasseGeschaetzt():
    global ui
    g_MassePaket = float(ui.EingabeKartonGewicht.text())
    return g_MassePaket

def UR_PickOffsetX():
    global ui
    g_Pick_Offset_X = int(ui.EingabeVerschiebungX.value())
    return g_Pick_Offset_X

def UR_PickOffsetY():
    global ui
    g_Pick_Offset_Y = int(ui.EingabeVerschiebungY.value())
    return g_Pick_Offset_Y

################################################################
# idk mit den scnnern
def UR_scanner1and2niobild():
    # TODO: Implement show image function to show the scanner image 1and2niobild
    return
def UR_scanner1bild():
    # TODO: Implement show image function to show the scanner image 1niobild
    return
def UR_scanner2bild():
    # TODO: Implement show image function to show the scanner image 2niobild
    return
def UR_scanner1and2iobild():
    # TODO: Implement show image function to show the scanner image 1and2iobild
    return
################################################################

def UR_Quergreifen():
    global ui
    logger.debug(f"{ui.checkBoxEinzelpaket.isChecked()=}")
    return ui.checkBoxEinzelpaket.isChecked()
                 
def Server_start():
    #server_stop_btn.configure(state="normal")
    global server
    server = SimpleXMLRPCServer(("", 8080), allow_none=True)
    print ("Start Server")
    server.register_function(UR_SetFileName, "UR_SetFileName")
    server.register_function(UR_ReadDataFromUsbStick, "UR_ReadDataFromUsbStick")
    server.register_function(UR_Palette, "UR_Palette")
    server.register_function(UR_Karton, "UR_Karton")
    server.register_function(UR_Lagen, "UR_Lagen")
    server.register_function(UR_Zwischenlagen, "UR_Zwischenlagen")
    server.register_function(UR_PaketPos, "UR_PaketPos")
    server.register_function(UR_AnzLagen, "UR_AnzLagen")
    server.register_function(UR_AnzPakete, "UR_AnzPakete") #Veraltet - nicht mehr verwenden
    server.register_function(UR_PaketeZuordnung, "UR_PaketeZuordnung") #Picks pro Lage
    server.register_function(UR_Paket_hoehe, "UR_Paket_hoehe") #Gemessene Pakethöhe
    server.register_function(UR_Startlage, "UR_Startlage") #Startlage für Neustart
    server.register_function(UR_Quergreifen, "UR_Quergreifen") #quer oder längs greifen
    server.register_function(UR_CoG, "UR_CoG") #Rückgabe von y und z
    server.register_function(UR_MasseGeschaetzt, "UR_MasseGeschaetzt") #Schätzen des Paketgewichts mit empirischem Faktor
    server.register_function(UR_PickOffsetX, "UR_PickOffsetX")
    server.register_function(UR_PickOffsetY, "UR_PickOffsetY")#Pick-Offset zur live Korrektur
    server.register_function(UR_StepBack, "UR_StepBack") #Audiosignal für Laserscanner
    server.register_function(UR_scanner1and2niobild, "UR_scanner1and2niobild")
    server.register_function(UR_scanner1bild, "UR_scanner1bild")
    server.register_function(UR_scanner2bild, "UR_scanner2bild")
    server.register_function(UR_scanner1and2iobild, "UR_scanner1and2iobild")
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
        server_address = (robot_ip, 29999)
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
        server_address = (robot_ip, 29999)
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
        server_address = (robot_ip, 29999)
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
    global ui
    ui.stackedWidget.setCurrentIndex(2)

def open_parameter_page() -> None:
    # set page of the stacked widgets to index 1
    global ui
    ui.tabWidget.setCurrentIndex(0)
    ui.stackedWidget.setCurrentIndex(1)

def open_main_page() -> None:
    # set page of the stacked widgets to index 0
    global ui
    ui.stackedWidget.setCurrentIndex(0)

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
    global ui
    Artikelnummer = ui.EingabePallettenplan.text()
    UR_SetFileName(Artikelnummer)
    
    errorReadDataFromUsbStick = UR_ReadDataFromUsbStick()

    if errorReadDataFromUsbStick == 1:
        logger.error(f"Error reading file for {Artikelnummer=} no file found")
        ui.LabelPalletenplanInfo.setText("Kein Plan gefunden")
        ui.LabelPalletenplanInfo.setStyleSheet("color: red")
    else:
        # remove the editing focus from the text box
        ui.EingabePallettenplan.clearFocus()
        logger.debug(f"File for {Artikelnummer=} found")
        ui.LabelPalletenplanInfo.setText("Plan erfolgreich geladen")
        ui.LabelPalletenplanInfo.setStyleSheet("color: green")
        ui.ButtonOpenParameterRoboter.setEnabled(True)
        ui.ButtonDatenSenden.setEnabled(True)
        ui.EingabeKartonGewicht.setEnabled(True)
        ui.EingabeKartonhoehe.setEnabled(True)
        ui.EingabeStartlage.setEnabled(True)
        ui.checkBoxEinzelpaket.setEnabled(True)

        Volumen = (g_PaketDim[0] * g_PaketDim[1] * g_PaketDim[2]) / 1E+9 # in m³
        logger.debug(f"{Volumen=}")
        Dichte = 1000 # Dichte von Wasser in kg/m³
        logger.debug(f"{Dichte=}")
        Ausnutzung = 0.4 # Empirsch ermittelter Faktor - nicht für Gasflaschen
        logger.debug(f"{Ausnutzung=}")
        Gewicht = round(Volumen * Dichte * Ausnutzung, 1) # Gewicht in kg
        logger.debug(f"{Gewicht=}")
        ui.EingabeKartonGewicht.setText(str(Gewicht))
        ui.EingabeKartonhoehe.setText(str(g_PaketDim[2]))

def send_data() -> None:
    global ui
    Server_thread()

def load_wordlist() -> list:
    wordlist = []
    count = 0
    for file in os.listdir(PATH_USB_STICK):
        if file.endswith(".rob"):
            wordlist.append(file[:-4])
            count = count + 1
    logger.debug(f"{count=}")
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
    global PATH_USB_STICK
    parser = argparse.ArgumentParser(description="Multipack Parser Application")
    parser.add_argument('--version', action='store_true', help='Show version information and exit')
    parser.add_argument('-v', '--verbose', action='store_true', help='Enable verbose logging')
    parser.add_argument('--rob-path', type=str, help='Path to the .rob files')
    args = parser.parse_args()

    if args.version:
        print(f"Multipack Parser Application Version: {VERSION}")
        return
    if args.verbose:
        # enable verbose logging
        logger.setLevel(logging.DEBUG)
    if args.rob_path:
        # try to check if the path is valid
        if os.path.exists(args.rob_path):
            # set the path to the .rob files
            PATH_USB_STICK = args.rob_path
        else:
            logger.error(f"Path {args.rob_path} does not exist")
            return

    logger.debug(f"{sys.argv=}")
    logger.debug(f"{VERSION=}")
    logger.debug(f"{PATH_USB_STICK=}")

    app = QApplication(sys.argv)
    main_window = QMainWindow()
    global ui
    ui = Ui_Form()
    ui.setupUi(main_window)
    ui.stackedWidget.setCurrentIndex(0)

    wordlist = load_wordlist()
    completer = QCompleter(wordlist, main_window)
    ui.EingabePallettenplan.setCompleter(completer)

    # Apply QIntValidator to restrict the input to only integers
    int_validator = QIntValidator()
    ui.EingabeKartonhoehe.setValidator(int_validator)

    # Apply CustomDoubleValidator to restrict the input to only numbers
    float_validator = CustomDoubleValidator()
    float_validator.setNotation(QDoubleValidator.StandardNotation)
    float_validator.setDecimals(2)  # Set to desired number of decimals
    ui.EingabeKartonGewicht.setValidator(float_validator)

    # if the user entered a Artikelnummer in the text box and presses enter it calls the load function
    ui.EingabePallettenplan.returnPressed.connect(load)

    #Page 1 Buttons
    ui.ButtonSettings.clicked.connect(open_password_dialog)
    ui.LadePallettenplan.clicked.connect(load)
    ui.ButtonOpenParameterRoboter.clicked.connect(open_parameter_page)
    ui.ButtonDatenSenden.clicked.connect(send_data)

    #Page 2 Buttons
    # Roboter Tab
    ui.ButtonZurueck.clicked.connect(open_main_page)
    ui.ButtonRoboterStart.clicked.connect(Send_cmd_play)
    ui.ButtonRoboterPause.clicked.connect(Send_cmd_pause)
    ui.ButtonRoboterStop.clicked.connect(Send_cmd_stop)
    ui.ButtonStopRPCServer.clicked.connect(Server_stop)
    # Aufnahme Tab
    ui.ButtonZurueck_2.clicked.connect(open_main_page)
    ui.ButtonDatenSenden_2.clicked.connect(send_data)

    # Page 3 Buttons
    ui.ButtonZurueck_3.clicked.connect(open_main_page)
    ui.Button_OpenExplorer.clicked.connect(open_explorer)
    ui.Button_OpenTerminal.clicked.connect(open_terminal)

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
