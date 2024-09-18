# port all functions from main.py that start with UR_ to this file and then import them into main.py to clean up the code
# This file will contain all the functions that are used to communicate with the UR10 robot
# This file will be imported into main.py to clean up the code

from . import global_vars

#Dateiname abfragen
def UR_SetFileName(Artikelnummer):
 
    global_vars.FILENAME = (Artikelnummer + '.rob')
    global_vars.logger.debug(f"{global_vars.FILENAME=}")
    return global_vars.FILENAME 
 
 
#daten vom usb stick hochladen und lesbar machen 
def UR_ReadDataFromUsbStick():
    global_vars.g_Daten = []
    global_vars.g_LageZuordnung = []
    global_vars.g_PaketPos = []
    global_vars.g_PaketeZuordnung = []
    global_vars.g_Zwischenlagen = []
    global_vars.g_paket_quer = 1
    global_vars.g_CenterOfGravity = [0,0,0]
    
    
    try:
        with open(global_vars.PATH_USB_STICK + global_vars.FILENAME) as file:
            
            for line in file:
                str = line.strip()
                tmpList = line.split('\t')
                
                for i in range(len(tmpList)):
                    tmpList[i] = int(tmpList[i])
                    
                global_vars.g_Daten.append(tmpList)
 
 
            pl = global_vars.g_Daten[global_vars.LI_PALETTE_DATA][global_vars.LI_PALETTE_DATA_LENGTH]
            pw = global_vars.g_Daten[global_vars.LI_PALETTE_DATA][global_vars.LI_PALETTE_DATA_WIDTH]
            ph = global_vars.g_Daten[global_vars.LI_PALETTE_DATA][global_vars.LI_PALETTE_DATA_HEIGHT]
            global_vars.g_PalettenDim = [pl, pw, ph]
            
            #Kartondaten
            pl = global_vars.g_Daten[global_vars.LI_PACKAGE_DATA][global_vars.LI_PACKAGE_DATA_LENGTH]
            pw = global_vars.g_Daten[global_vars.LI_PACKAGE_DATA][global_vars.LI_PACKAGE_DATA_WIDTH]
            ph = global_vars.g_Daten[global_vars.LI_PACKAGE_DATA][global_vars.LI_PACKAGE_DATA_HEIGHT]
            pr = global_vars.g_Daten[global_vars.LI_PACKAGE_DATA][global_vars.LI_PACKAGE_DATA_GAP]
            global_vars.g_PaketDim = [pl, pw, ph, pr]
            
            #Lagearten
            global_vars.g_LageArten = global_vars.g_Daten[global_vars.LI_LAYERTYPES][0]
            
            #Lagenzuordnung
            anzLagen = global_vars.g_Daten[global_vars.LI_NUMBER_OF_LAYERS][0]
            global_vars.g_AnzLagen = anzLagen
 
 
            index       = global_vars.LI_NUMBER_OF_LAYERS + 2
            end_index   = index + anzLagen
 
 
            while index < end_index:
                
                lagenart = global_vars.g_Daten[index][0]
                zwischenlagen = global_vars.g_Daten[index][1]
 
                global_vars.g_LageZuordnung.append(lagenart)
                global_vars.g_Zwischenlagen.append(zwischenlagen)
            
                index = index +1
            
            #Paketpositionen
            ersteLage   = 4 + (anzLagen + 1)
            index       = ersteLage
            anzahlPaket = global_vars.g_Daten[index][0]
            global_vars.g_AnzahlPakete = anzahlPaket #Achtung veraltet - Anzahl der Picks bei Multipick
            index_paketZuordnung = index
            
            for i in range(global_vars.g_LageArten):
                
                anzahlPick = global_vars.g_Daten[index_paketZuordnung][0]
                global_vars.g_PaketeZuordnung.append(anzahlPick)
                index_paketZuordnung = index_paketZuordnung + anzahlPick + 1
                
            
            for i in range(global_vars.g_LageArten):            
                index = index + 1 #Überspringe die Zeile mit der Anzahl der Pakete
                anzahlPaket = global_vars.g_PaketeZuordnung[i]
                
                for j in range(anzahlPaket):
                    xp = global_vars.g_Daten[index][global_vars.LI_POSITION_XP]
                    yp = global_vars.g_Daten[index][global_vars.LI_POSITION_YP]
                    ap = global_vars.g_Daten[index][global_vars.LI_POSITION_AP]
                    xd = global_vars.g_Daten[index][global_vars.LI_POSITION_XD]
                    yd = global_vars.g_Daten[index][global_vars.LI_POSITION_YD]
                    ad = global_vars.g_Daten[index][global_vars.LI_POSITION_AD]
                    nop = global_vars.g_Daten[index][global_vars.LI_POSITION_NOP]
                    xvec = global_vars.g_Daten[index][global_vars.LI_POSITION_XVEC]
                    yvec = global_vars.g_Daten[index][global_vars.LI_POSITION_YVEC]
                    packagePos = [xp, yp, ap, xd, yd, ad, nop, xvec, yvec]
                    global_vars.g_PaketPos.append(packagePos)
                    index = index + 1    
 
            return 0                
    except:
        global_vars.logger.error(f"Error reading file {global_vars.FILENAME}")
    return 1
 
 
#funktion für den roboter 
def UR_Palette():
    return global_vars.g_PalettenDim
 
def UR_Karton():
    return global_vars.g_PaketDim
 
def UR_Lagen():
    return global_vars.g_LageZuordnung
 
def UR_Zwischenlagen():
    return global_vars.g_Zwischenlagen
 
def UR_PaketPos(Nummer):
    return global_vars.g_PaketPos[Nummer]
 
def UR_AnzLagen():
    return global_vars.g_AnzLagen
 
def UR_AnzPakete():
    return global_vars.g_AnzahlPakete
 
def UR_PaketeZuordnung():
    return global_vars.g_PaketeZuordnung
 
 
#den "center of gravity" messen
def UR_CoG(Masse_Paket,Masse_Greifer,Anzahl_Pakete=1):
 
    if(Anzahl_Pakete == 0):
        Masse_Paket=0
    #Berechnung Y
    Karton_Y = global_vars.g_PaketDim[0]
    y = (1/(Masse_Greifer + Masse_Paket))*((-0.045*Masse_Greifer)+(-0.045*Masse_Paket))
    #Berechnung Z
    Karton_Z = global_vars.g_PaketDim[2]
    z = (1/(Masse_Greifer + (Masse_Paket*Anzahl_Pakete)))*((0.047*Masse_Greifer)+((0.047+(Karton_Z/2000))*Masse_Paket*Anzahl_Pakete)) #Annahme Schwerpunkt Paket ist halbe Höhe u. mm zu m -> Karton_Z/2*1000
    #Zuweisung Array
    global_vars.g_CenterOfGravity[0] = y
    global_vars.g_CenterOfGravity[1] = z
    return global_vars.g_CenterOfGravity
 
    
#die funktion für den audio file zu spielen
def UR_StepBack():
    file = AudioSegment.from_file(file = PATH_BILDER + "stepback.mp3", format = "mp3")
    play(file)
    return


def UR_Paket_hoehe():
    #global_vars.ui
    global_vars.g_PaketDim[2] = int(global_vars.ui.EingabeKartonhoehe.text())
    return global_vars.g_PaketDim[2]

def UR_Startlage():
    #global_vars.ui
    global_vars.g_Startlage = int(global_vars.ui.EingabeStartlage.value())
    return global_vars.g_Startlage

def UR_MasseGeschaetzt():
    #global_vars.ui
    global_vars.g_MassePaket = float(global_vars.ui.EingabeKartonGewicht.text())
    return global_vars.g_MassePaket

def UR_PickOffsetX():
    #global_vars.ui
    global_vars.g_Pick_Offset_X = int(global_vars.ui.EingabeVerschiebungX.value())
    return global_vars.g_Pick_Offset_X

def UR_PickOffsetY():
    #global_vars.ui
    global_vars.g_Pick_Offset_Y = int(global_vars.ui.EingabeVerschiebungY.value())
    return global_vars.g_Pick_Offset_Y

################################################################
# idk mit den scnnern
def UR_scanner1and2niobild():
    #global_vars.ui
    global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR10e/imgs/scanner1&2nio.png'))
    return
def UR_scanner1bild():
    #global_vars.ui
    global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR10e/imgs/scanner1nio.png'))
    return
def UR_scanner2bild():
    #global_vars.ui
    global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR10e/imgs/scanner2nio.png'))
    return
def UR_scanner1and2iobild():
    #global_vars.ui
    global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR10e/imgs/scannerio.png'))
    return
################################################################

def UR_Quergreifen():
    #global_vars.ui
    global_vars.logger.debug(f"{global_vars.ui.checkBoxEinzelpaket.isChecked()=}")
    return global_vars.ui.checkBoxEinzelpaket.isChecked()