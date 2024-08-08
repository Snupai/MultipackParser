#alles was man für den programm brauch(PyQt, threading, etc..)

import threading
import time
from PyQt5 import QtCore, QtGui, QtWidgets
from random import randint
from PyQt5.QtWidgets import QApplication, QMainWindow
from PyQt5.QtWidgets import QPushButton, QVBoxLayout, QLabel
from PyQt5.QtGui import QMouseEvent
from PyQt5.QtCore import QTimer
from xmlrpc.server import SimpleXMLRPCServer
from pydub import AudioSegment
from pydub.playback import play
import re
import xmlrpc.client
import socket
import sys
import os

##############
# Konstanten #
##############

VERSION = '1.1.0' # Version des Programmes - Änderungen an den Funktionen und/oder der Benutzung muss die Version angepasst werden

# IP Address of the Robot - Set to localhost only for testing
robot_ip = '192.168.0.1' # DO NOT CHANGE


# Konstanten
#PATH_USB_STICK     = 'E:\'
PATH_USB_STICK  = '' 
PATH_BILDER = f'{os.path.dirname(os.path.realpath(__file__))}/imgs/' # Bekomme den Pfad zu diesem Skript und setze den Pfad auf den Ordner cwd/imgs/

if PATH_USB_STICK == '':
    # Bekomme CWD und setze den Pfad auf den Überordner
    PATH_USB_STICK = f'{os.path.dirname(os.path.dirname(os.path.realpath(__file__)))}/' 

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

##############
# Funktionen #
##############


#Dateiname abfragen
def UR_SetFileName(Artikelnummer):

    global FILENAME
    
    FILENAME = (Artikelnummer + '.rob')
    #print(FILENAME)    
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
        print("Error")
        print(FILENAME)
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


#die classe "ui_palletierer"    
class Ui_palletierer(object):
    #das ist die user interface 
    def setupUi(self, palletierer):
        
        palletierer.setObjectName("palletierer")
        palletierer.setEnabled(True)
        palletierer.resize(1368, 389)    #das fenster neu plazieren

        sizePolicy = QtWidgets.QSizePolicy(QtWidgets.QSizePolicy.Preferred, QtWidgets.QSizePolicy.Preferred)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(palletierer.sizePolicy().hasHeightForWidth())  

        palletierer.setSizePolicy(sizePolicy)
        font = QtGui.QFont()
        font.setFamily("MS UI Gothic")
        font.setPointSize(22)
        palletierer.setFont(font)

        self.centralwidget = QtWidgets.QWidget(palletierer)
        self.centralwidget.setObjectName("centralwidget")

        self.stackedWidget = QtWidgets.QStackedWidget(self.centralwidget)
        self.stackedWidget.setGeometry(QtCore.QRect(0, 0, 1366, 389))
        self.stackedWidget.setMouseTracking(False)
        self.stackedWidget.setAutoFillBackground(True)
        self.stackedWidget.setLineWidth(3)
        self.stackedWidget.setObjectName("stackedWidget")

        self.FIRSTPAGE = QtWidgets.QWidget()
        self.FIRSTPAGE.setObjectName("FIRSTPAGE")    #die erste page 

        self.eingabelage = QtWidgets.QSpinBox(self.FIRSTPAGE)
        self.eingabelage.setGeometry(QtCore.QRect(410, 120, 471, 41))
        self.eingabelage.setObjectName("eingabelage")      #die schreib zone um die lage einzugeben
        
        self.eingabevonderhohe = QtWidgets.QLineEdit(self.FIRSTPAGE)
        self.eingabevonderhohe.setGeometry(QtCore.QRect(410, 180, 471, 41))
        self.eingabevonderhohe.setObjectName("eingabevonderhohe")       #die schreib zone um die hohe einzugeben
        

        self.eingabevomgewicht = QtWidgets.QLineEdit(self.FIRSTPAGE)
        self.eingabevomgewicht.setGeometry(QtCore.QRect(410, 240, 471, 41))
        self.eingabevomgewicht.setAutoFillBackground(True)
        self.eingabevomgewicht.setObjectName("eingabevomgewicht")       #die schreib zone um den gewicht einzugeben

        self.eingabevompaletierplan = QtWidgets.QLineEdit(self.FIRSTPAGE)
        self.eingabevompaletierplan.setGeometry(QtCore.QRect(410, 60, 471, 41))
        font = QtGui.QFont()
        font.setFamily("MS UI Gothic")
        font.setPointSize(36)
        self.eingabevompaletierplan.setFont(font)
        self.eingabevompaletierplan.setCursor(QtGui.QCursor(QtCore.Qt.CrossCursor))
        self.eingabevompaletierplan.setText("")
        self.eingabevompaletierplan.setObjectName("eingabevompaletierplan")         #die schreib zone um den pallettenplan einzugeben

        self.checkBox = QtWidgets.QCheckBox(self.FIRSTPAGE)
        self.checkBox.setGeometry(QtCore.QRect(410, 290, 121, 41))
        self.checkBox.setText("")
        self.checkBox.setIconSize(QtCore.QSize(40, 40))
        self.checkBox.setTristate(False)
        self.checkBox.setObjectName("checkBox")     #die check box um quer zu greifen 

        self.paletierplan = QtWidgets.QLabel(self.FIRSTPAGE)
        self.paletierplan.setGeometry(QtCore.QRect(0, 60, 401, 51))
        font = QtGui.QFont()
        font.setFamily("MS UI Gothic")
        font.setPointSize(28)
        self.paletierplan.setFont(font)
        self.paletierplan.setObjectName("paletierplan")         #schreiben paletierplan

        self.startlage = QtWidgets.QLabel(self.FIRSTPAGE)
        self.startlage.setGeometry(QtCore.QRect(0, 120, 401, 41))
        font = QtGui.QFont()
        font.setFamily("MS UI Gothic")
        font.setPointSize(28)
        self.startlage.setFont(font)
        self.startlage.setObjectName("startlage")       #schreiben startlage

        self.kartonhohewahlen = QtWidgets.QLabel(self.FIRSTPAGE)
        self.kartonhohewahlen.setGeometry(QtCore.QRect(0, 180, 411, 41))
        font = QtGui.QFont()
        font.setFamily("MS UI Gothic")
        font.setPointSize(28)
        self.kartonhohewahlen.setFont(font)
        self.kartonhohewahlen.setObjectName("kartonhohe")     #schreiben kartonhohewahlen

        self.gewichtwahlen = QtWidgets.QLabel(self.FIRSTPAGE)
        self.gewichtwahlen.setGeometry(QtCore.QRect(0, 230, 401, 50))
        font = QtGui.QFont()
        font.setFamily("MS UI Gothic")
        font.setPointSize(28)
        self.gewichtwahlen.setFont(font)
        self.gewichtwahlen.setObjectName("gewicht")       #schreiben gewichtwahlen

        self.einzelpacketquer = QtWidgets.QLabel(self.FIRSTPAGE)
        self.einzelpacketquer.setGeometry(QtCore.QRect(-10, 290, 411, 41))
        font = QtGui.QFont()
        font.setFamily("MS UI Gothic")
        font.setPointSize(28)
        self.einzelpacketquer.setFont(font)
        self.einzelpacketquer.setObjectName("einzelpacketquer")     #schreiben einzelpaket quer

        self.keinplangeladen = QtWidgets.QLabel(self.FIRSTPAGE)
        self.keinplangeladen.setGeometry(QtCore.QRect(450, 10, 351, 41))
        self.keinplangeladen.setObjectName("keinplangeladen")       #schreiben keinplangeladen

        self.mm = QtWidgets.QLabel(self.FIRSTPAGE)
        self.mm.setGeometry(QtCore.QRect(890, 190, 60, 31))
        self.mm.setObjectName("mm")         #schreiben kg

        self.kg = QtWidgets.QLabel(self.FIRSTPAGE)
        self.kg.setGeometry(QtCore.QRect(890, 250, 61, 50))
        self.kg.setObjectName("kg")         #schreiben kg

        self.parameterroboter = QtWidgets.QPushButton(self.FIRSTPAGE)
        self.parameterroboter.setGeometry(QtCore.QRect(450, 300, 391, 61))
        self.parameterroboter.setObjectName("parameterroboter")     #pushbutton um die parameter von dem roboter zu öffnen

        self.logoszaidel = QtWidgets.QLabel(self.FIRSTPAGE)
        self.logoszaidel.setGeometry(QtCore.QRect(1020, -90, 301, 461))
        self.logoszaidel.setText("")
        self.logoszaidel.setPixmap(QtGui.QPixmap(PATH_BILDER + "szaidel-transparent.png"))
        self.logoszaidel.setObjectName("logoszaidel")       #bild von szaidel first page rechts

        self.uploadimg = QtWidgets.QLabel(self.FIRSTPAGE)
        self.uploadimg.setGeometry(QtCore.QRect(900, 50, 51, 61))
        self.uploadimg.setText("")
        self.uploadimg.setPixmap(QtGui.QPixmap(PATH_BILDER + "load.png"))
        self.uploadimg.setObjectName("uploadimg")       #bild um hochzuladen

        self.datenamrobotersenden = QtWidgets.QPushButton(self.FIRSTPAGE)
        self.datenamrobotersenden.setGeometry(QtCore.QRect(860, 300, 401, 61))
        self.datenamrobotersenden.setObjectName("datenamrobotersenden")     #pushbutton daten am roboter senden (server start)

        self.stackedWidget.addWidget(self.FIRSTPAGE)

        self.ROBOTER = QtWidgets.QWidget()
        self.ROBOTER.setObjectName("ROBOTER")           # das sit die widget wenn man den pushbutton parameterroboter druckt 

        self.tabWidget_2 = QtWidgets.QTabWidget(self.ROBOTER)
        self.tabWidget_2.setGeometry(QtCore.QRect(0, 0, 1371, 381))
        self.tabWidget_2.setAutoFillBackground(False)
        self.tabWidget_2.setTabPosition(QtWidgets.QTabWidget.North)
        self.tabWidget_2.setObjectName("tabWidget_2")

        self.roboter = QtWidgets.QWidget()
        self.roboter.setObjectName("roboter")

        self.roboterstart = QtWidgets.QPushButton(self.roboter)
        self.roboterstart.setGeometry(QtCore.QRect(180, 60, 361, 101))

        sizePolicy = QtWidgets.QSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        sizePolicy.setHorizontalStretch(200)
        sizePolicy.setVerticalStretch(90)
        sizePolicy.setHeightForWidth(self.roboterstart.sizePolicy().hasHeightForWidth())

        self.roboterstart.setSizePolicy(sizePolicy)
        self.roboterstart.setIconSize(QtCore.QSize(100, 100))
        self.roboterstart.setObjectName("roboterstart")             # pushbutton das der roboter starten soll

        self.roboterstop = QtWidgets.QPushButton(self.roboter)
        self.roboterstop.setGeometry(QtCore.QRect(770, 60, 361, 101))

        sizePolicy = QtWidgets.QSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        sizePolicy.setHorizontalStretch(200)
        sizePolicy.setVerticalStretch(90)
        sizePolicy.setHeightForWidth(self.roboterstop.sizePolicy().hasHeightForWidth())

        self.roboterstop.setSizePolicy(sizePolicy)
        self.roboterstop.setIconSize(QtCore.QSize(100, 100))
        self.roboterstop.setObjectName("roboterstop")       # pushbutton das der roboter stoppen soll

        self.stoprpcserver = QtWidgets.QPushButton(self.roboter)
        self.stoprpcserver.setGeometry(QtCore.QRect(770, 190, 361, 101))

        sizePolicy = QtWidgets.QSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        sizePolicy.setHorizontalStretch(200)
        sizePolicy.setVerticalStretch(90)
        sizePolicy.setHeightForWidth(self.stoprpcserver.sizePolicy().hasHeightForWidth())

        self.stoprpcserver.setSizePolicy(sizePolicy)
        self.stoprpcserver.setIconSize(QtCore.QSize(100, 100))
        self.stoprpcserver.setObjectName("stoprpcserver")           # pushbutton das der server stoppen soll

        self.roboterpause = QtWidgets.QPushButton(self.roboter)
        self.roboterpause.setGeometry(QtCore.QRect(180, 190, 361, 101))

        sizePolicy = QtWidgets.QSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        sizePolicy.setHorizontalStretch(200)
        sizePolicy.setVerticalStretch(90)
        sizePolicy.setHeightForWidth(self.roboterpause.sizePolicy().hasHeightForWidth())

        self.roboterpause.setSizePolicy(sizePolicy)
        self.roboterpause.setIconSize(QtCore.QSize(100, 100))
        self.roboterpause.setObjectName("roboterpause")         # pushbutton das der roboter pause macht

        self.szaidelbackground = QtWidgets.QLabel(self.roboter)
        self.szaidelbackground.setGeometry(QtCore.QRect(330, -20, 881, 331))
        self.szaidelbackground.setText("")
        self.szaidelbackground.setPixmap(QtGui.QPixmap(PATH_BILDER + "logoszaidel-transparent-big.png"))
        self.szaidelbackground.setObjectName("szaidelbackground")           #bild von szaidel im hintergrund

        self.zurucklogo_2 = QtWidgets.QLabel(self.roboter)
        self.zurucklogo_2.setGeometry(QtCore.QRect(0, 110, 71, 71))
        self.zurucklogo_2.setText("")
        self.zurucklogo_2.setPixmap(QtGui.QPixmap(PATH_BILDER + "zurueck.png"))
        self.zurucklogo_2.setObjectName("zurucklogo_2")         #bild lings vom fenster um auf firstpage zu gehen 

        self.scanner1nio = QtWidgets.QLabel(self.FIRSTPAGE)
        self.scanner1nio.setGeometry(QtCore.QRect(0, 0, 0 ,0))
        self.scanner1nio.setPixmap(QtGui.QPixmap(PATH_BILDER + "scanner1nio.png"))
        self.scanner1nio.setObjectName("scanner1nio")     
        

        self.scanner2nio = QtWidgets.QLabel(self.FIRSTPAGE)
        self.scanner2nio.setGeometry(QtCore.QRect(0, 0, 0 ,0))
        self.scanner2nio.setPixmap(QtGui.QPixmap(PATH_BILDER + "scanner2nio.png"))
        self.scanner2nio.setObjectName("scanner2nio")     
        

        self.scannernio = QtWidgets.QLabel(self.FIRSTPAGE)
        self.scannernio.setGeometry(QtCore.QRect(0, 0, 0 ,0))                                  # das sind die bilder die angezeigt werden falls jemand den scanner weg nimmt 
        self.scannernio.setPixmap(QtGui.QPixmap(PATH_BILDER + "scanner1&2nio.png"))
        self.scannernio.setObjectName("scanner1&2nio")     
        
        self.scannerio = QtWidgets.QLabel(self.FIRSTPAGE)
        self.scannerio.setGeometry(QtCore.QRect(0, 0, 0, 0))
        self.scannerio.setPixmap(QtGui.QPixmap(PATH_BILDER + "scannerio.png"))
        self.scannerio.setObjectName("scannerio")     
        

        self.roboterstop.raise_()

        self.stoprpcserver.raise_()

        self.roboterpause.raise_()

        self.roboterstart.raise_()

        self.zurucklogo_2.raise_()

        self.fehlerbeimladen = QtWidgets.QLabel(self.FIRSTPAGE)
        self.fehlerbeimladen.setGeometry(QtCore.QRect(0, 0, 0, 0))
        self.fehlerbeimladen.setObjectName("Fehler beim laden")
        self.fehlerbeimladen.setStyleSheet("color: red;")               #schreiben fehler beim laden in rot 

        self.ladenerfolgreich = QtWidgets.QLabel(self.FIRSTPAGE)
        self.ladenerfolgreich .setGeometry(QtCore.QRect(0, 0, 0, 0))
        self.ladenerfolgreich .setObjectName("laden erfolgreich")
        self.ladenerfolgreich .setStyleSheet("color: green;")           #schreiben erfolgreich in grün

        self.tabWidget_2.addTab(self.roboter, "")

        self.aufnahme = QtWidgets.QWidget()
        self.aufnahme.setObjectName("aufnahme")

        self.datenX = QtWidgets.QPlainTextEdit(self.aufnahme)
        self.datenX.setGeometry(QtCore.QRect(1010, 40, 181, 51))
        self.datenX.setObjectName("DatenX")         #daten X eingeben
        self.datenX.setPlainText("0")
        

        self.verschiebenX = QtWidgets.QLabel(self.aufnahme)
        self.verschiebenX.setGeometry(QtCore.QRect(530, 40, 481, 61))
        self.verschiebenX.setObjectName("verschiebenX")     #schreiben verschieben in richtung x von

        self.verschiebenY = QtWidgets.QLabel(self.aufnahme)
        self.verschiebenY.setGeometry(QtCore.QRect(530, 160, 471, 51))
        self.verschiebenY.setObjectName("verschiebenY")     #schreiben verschieben in richtung x von

        self.mmX = QtWidgets.QLabel(self.aufnahme)
        self.mmX.setGeometry(QtCore.QRect(1200, 50, 80, 31))
        self.mmX.setObjectName("mmX")       #schreiben mmX

        self.mmY = QtWidgets.QLabel(self.aufnahme)
        self.mmY.setGeometry(QtCore.QRect(1200, 170, 80, 31))
        self.mmY.setObjectName("mmY")              #schreiben mmY

        self.datenY = QtWidgets.QPlainTextEdit(self.aufnahme)
        self.datenY.setGeometry(QtCore.QRect(1010, 160, 181, 51))
        self.datenY.setObjectName("datenY")     #daten y eingeben
        self.datenY.setPlainText("0")

        self.bildaufnahme = QtWidgets.QLabel(self.aufnahme)
        self.bildaufnahme.setGeometry(QtCore.QRect(70, -10, 481, 341))
        self.bildaufnahme.setText("")
        self.bildaufnahme.setPixmap(QtGui.QPixmap(PATH_BILDER + "Aufnahmepos.png"))
        self.bildaufnahme.setObjectName("bildaufnahme")         #bild wie der roboter den karton nimmt

        self.datenamrobotersenden2 = QtWidgets.QPushButton(self.aufnahme)
        self.datenamrobotersenden2.setGeometry(QtCore.QRect(530, 260, 721, 81))
        self.datenamrobotersenden2.setObjectName("datenamrobotersenden2")          #pushbutton daten senden 

        self.zurucklogo = QtWidgets.QLabel(self.aufnahme)
        self.zurucklogo.setGeometry(QtCore.QRect(0, 110, 71, 71))
        self.zurucklogo.setText("")
        self.zurucklogo.setPixmap(QtGui.QPixmap(PATH_BILDER +  "zurueck.png"))
        self.zurucklogo.setObjectName("zurucklogo")           # bild zuruck gehen

        icon = QtGui.QIcon.fromTheme("roboter")

        self.tabWidget_2.addTab(self.aufnahme, icon, "")
        self.stackedWidget.addWidget(self.ROBOTER)

        palletierer.setCentralWidget(self.centralwidget)

        self.retranslateUi(palletierer)

        self.stackedWidget.setCurrentIndex(0)

        self.tabWidget_2.setCurrentIndex(0)
    
        QtCore.QMetaObject.connectSlotsByName(palletierer)
     
        self.parameterroboter.clicked.connect(self.openwidgetroboter)
        self.zurucklogo.mousePressEvent = self.clickonbacktohome
        self.zurucklogo_2.mousePressEvent = self.clickonbacktohome
        self.roboterpause.mousePressEvent = self.roboterpausesenden
        self.roboterstart.mousePressEvent = self.roboterstartsenden                 #hier gebe ich jeden pushbutton oder bild eine fonction an wenn  jemand drauf druckt 
        self.roboterstop.mousePressEvent = self.roboterstopsenden
        self.datenamrobotersenden.mousePressEvent = self.datenrobotersenden
        self.datenamrobotersenden2.mousePressEvent = self.datenrobotersenden
        self.uploadimg.mousePressEvent = self.load
        self.parameterroboter.setEnabled(False)
        self.eingabelage.setEnabled(False)
        self.eingabevomgewicht.setEnabled(False)
        self.eingabevonderhohe.setEnabled(False)
        self.datenamrobotersenden.setEnabled(False)
        
    def Quergreifen(self):
        if self.checkBox.isTristate(1):
            return 1
        else:
            return 0
        
    def scannerio1(self):
        self.scannerio.setGeometry(QtCore.QRect(1020, -90, 301, 461))
        self.scannernio.setGeometry(QtCore.QRect(0, 0, 0, 0))
        self.scanner1nio.setGeometry(QtCore.QRect(0, 0, 0, 0))
        self.scanner2nio.setGeometry(QtCore.QRect(0, 0, 0, 0))
    def scanner1nio1(self):
        self.scannerio.setGeometry(QtCore.QRect(0, 0, 0, 0))
        self.scannernio.setGeometry(QtCore.QRect(0, 0, 0, 0))
        self.scanner1nio.setGeometry(QtCore.QRect(1020, -90, 301, 461))
        self.scanner2nio.setGeometry(QtCore.QRect(0, 0, 0, 0))
            
    def scanner2nio1(self):
        self.scannerio.setGeometry(QtCore.QRect(0, 0, 0, 0))
        self.scannernio.setGeometry(QtCore.QRect(0, 0, 0, 0))
        self.scanner1nio.setGeometry(QtCore.QRect(0, 0, 0, 0))
        self.scanner2nio.setGeometry(QtCore.QRect(1020, -90, 301, 461))
    
    def scanner1und2nio1(self):
        self.scannerio.setGeometry(QtCore.QRect(0, 0, 0, 0))
        self.scannernio.setGeometry(QtCore.QRect(1020, -90, 301, 461))
        self.scanner1nio.setGeometry(QtCore.QRect(0, 0, 0, 0))
        self.scanner2nio.setGeometry(QtCore.QRect(0, 0, 0, 0))
    def Paket_hoehe(self):
            
        g_PaketDim[2] = self.eingabevonderhohe.text()
        return int (g_PaketDim[2])

    def Startlage(self): 
              
        g_Startlage = self.eingabelage.text()                                       #hier geht es um die eingabe vom user an der variable zu geben 
        return int (g_Startlage)

    def MasseGeschaetzt(self):

        g_MassePaket = self.gewichtwahlen.text()
        return float(g_MassePaket)
    
    def PickOffsetX(self):
        
        Offset_X = int(self.datenX.toPlainText())#X Korrekturwert auslesen
             
                
        return Offset_X           
    def PickOffsetY(self):
        
        Offset_Y = int(self.datenY.toPlainText())#X Korrekturwert auslesen
             
                
        return Offset_Y           

    def datenrobotersenden(self, event):
        
        if event.button() == QtCore.Qt.LeftButton:
          
            if  str(self.datenX.toPlainText()) < "50" and str(self.datenY.toPlainText()) < "50":                    #die eingabe darf nicht 50 ubertreten
                
                Server_thread()
            
            else:
                
                QtWidgets.QMessageBox.critical(None, "Fehler", "vershiebung zu groß")    
    
        
    def retranslateUi(self, palletierer):
            _translate = QtCore.QCoreApplication.translate
            palletierer.setWindowTitle(_translate("palletierer", "Palletierer - Version " + VERSION))
            palletierer.setWindowIcon(QtGui.QIcon(PATH_BILDER + "pallet.ico"))
            self.paletierplan.setText(_translate("palletierer", "<html><head/><body><p align=\"right\">PALETIERPLAN :</p></body></html>"))
            self.startlage.setText(_translate("palletierer", "<html><head/><body><p align=\"right\">STARTLAGE  :</p></body></html>"))
            self.kartonhohewahlen.setText(_translate("palletierer", "             KARTONHOHE :"))
            self.gewichtwahlen.setText(_translate("palletierer", "<html><head/><body><p align=\"right\">    GEWICHT :</p></body></html>"))
            self.einzelpacketquer.setText(_translate("palletierer", "<html><head/><body><p align=\"right\"> EINZELPAKET :</p></body></html>"))
            self.keinplangeladen.setText(_translate("palletierer", "<html><head/><body><p><span style=\" font-size:28pt;\">KEIN PLAN GELADEN </span></p></body></html>"))
            self.mm.setText(_translate("palletierer", "mm"))
            self.kg.setText(_translate("palletierer", "kg"))
            self.fehlerbeimladen.setText(_translate("palletierer", "FEHLER BEIM DEM HOCHLADEN"))
            self.ladenerfolgreich.setText(_translate("palletierer", "HOCHLADEN ERFOLGREICH"))                            #das ist alles was auf dem user interface geschrieben ist 
            self.parameterroboter.setText(_translate("palletierer", "PARAMETER ROBOTER "))
            self.datenamrobotersenden2.setText(_translate("palletierer", "DATEN SENDEN"))
            self.roboterstart.setText(_translate("palletierer", "ROBOTER START"))
            self.roboterstop.setText(_translate("palletierer", "ROBOTER STOP"))
            self.stoprpcserver.setText(_translate("palletierer", "STOP RPC SERVER"))
            self.roboterpause.setText(_translate("palletierer", "ROBOTER PAUSE"))
            self.tabWidget_2.setTabText(self.tabWidget_2.indexOf(self.roboter), _translate("palletierer", "ROBOTER"))
            self.verschiebenX.setText(_translate("palletierer", "verschieben in richtung x von : "))
            self.verschiebenY.setText(_translate("palletierer", "verschieben in richtung y von : "))
            self.mmX.setText(_translate("palletierer", "mmX"))
            self.mmY.setText(_translate("palletierer", "mmY"))
         
            self.datenamrobotersenden.setText(_translate("palletierer", "DATEN SENDEN"))
            self.tabWidget_2.setTabText(self.tabWidget_2.indexOf(self.aufnahme), _translate("palletierer", "AUFNAHME"))
              
    def openwidgetroboter(self):
            self.stackedWidget.setCurrentIndex(1)           #funktion um in den user interface zu spazieren können
    def clickonbacktohome(self,event):
            if event.button() == QtCore.Qt.LeftButton:                
                self.stackedWidget.setCurrentIndex(0)
    def load(self, event):              #um den palletten plan hochzuladen
        
        if event.button() == QtCore.Qt.LeftButton:
            plan = self.eingabevompaletierplan.text()
            UR_SetFileName(plan)
             
            Err = UR_ReadDataFromUsbStick()
            
        
        

        if Err == 1:
            self.keinplangeladen.setGeometry(QtCore.QRect(0, 0, 0, 0))
            self.fehlerbeimladen.setGeometry(QtCore.QRect(0, 0, 0, 0))
            self.ladenerfolgreich.setGeometry(QtCore.QRect(0, 0, 0, 0))
            self.fehlerbeimladen.setGeometry(QtCore.QRect(440, 10, 441, 51))
            QtWidgets.QMessageBox.critical(None, "Fehler", ">>PALLETTEN PLAN NICHT GEFUNDEN<<")
            self.fehlerbeimladen.raise_()
            
            

        else:
            self.keinplangeladen.setGeometry(QtCore.QRect(0, 0, 0, 0))
            self.fehlerbeimladen.setGeometry(QtCore.QRect(0, 0, 0, 0))
            self.ladenerfolgreich.setGeometry(QtCore.QRect(460, 10, 441, 51))
            self.parameterroboter.setEnabled(True)
            self.parameterroboter.setEnabled(True)
            self.eingabelage.setEnabled(True)
            self.eingabevomgewicht.setEnabled(True)
            self.eingabevonderhohe.setEnabled(True)
            self.datenamrobotersenden.setEnabled(True)
            self.eingabelage.setValue(1)
            Volumen = (g_PaketDim[0] * g_PaketDim[1] * g_PaketDim[2]) / 1E+9  # Volumen in m³           
            Dichte = 1000  # Dichte von Wasser in kg/m³
            Ausnutzung = 0.4  # Empirsch ermittelter Faktor - Nicht für Glasflaschen!
            Gew = round(Volumen * Dichte * Ausnutzung, 1)
            self.eingabevonderhohe.setText(str(g_PaketDim[2]))
            self.eingabevomgewicht.setText(str(Gew))
            
   
                
    def roboterstartsenden(self, event):
         if event.button() == QtCore.Qt.LeftButton:        
              Send_cmd_play()      
    def roboterstopsenden(self, event):
         if event.button() == QtCore.Qt.LeftButton:                 #funktion fur die daten auf dem server zu schicken
              Send_cmd_stop()        
    def roboterpausesenden(self, event):
         if event.button() == QtCore.Qt.LeftButton:        
              Send_cmd_pause()        
    def roboterserverstopsenden(self, event):
            if event.button() == QtCore.Qt.LeftButton:        
              Server_stop()

   

         
ui= Ui_palletierer()

def UR_Paket_hoehe():
    return ui.Paket_hoehe()
def UR_Startlage():
    return ui.Startlage()
def UR_MasseGeschaetzt():
    return ui.MasseGeschaetzt()
def UR_PickOffsetX():
    return ui.PickOffsetX()
def UR_PickOffsetY():
    return ui.PickOffsetY()
def UR_scanner1and2niobild():
    return ui.scannerio1()
def UR_scanner1bild():
    return ui.scanner1nio1()
def UR_scanner2bild():
    return ui.scanner2nio1()
def UR_scanner1and2iobild():
    return ui.scanner1und2nio1()
def UR_Quergreifen():
    return ui.Quergreifen()
                 
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

                    
                 




if __name__ == "__main__":
    import sys
    app = QApplication(sys.argv)
    palletierer = QMainWindow()
    ui = Ui_palletierer()
    ui.setupUi(palletierer)
    palletierer.show()
    sys.exit(app.exec_())