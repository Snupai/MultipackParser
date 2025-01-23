# port all functions from main.py that start with UR_ to this file and then import them into main.py to clean up the code
# This file will contain all the functions that are used to communicate with the UR10 robot
# This file will be imported into main.py to clean up the code

from . import global_vars
from pydub import AudioSegment
from pydub.playback import play
from PySide6.QtGui import QPixmap

logger = global_vars.logger

#Dateiname abfragen
def UR_SetFileName(Artikelnummer):
    """
    Set the filename.

    Args:
        Artikelnummer (str): The article number.

    Returns:
        str: The filename.
    """
    global_vars.FILENAME = (Artikelnummer + '.rob')
    logger.debug(f"{global_vars.FILENAME=}")
    return global_vars.FILENAME 
 
 
#daten vom usb stick hochladen und lesbar machen 
def UR_ReadDataFromUsbStick():
    """
    Read data from the Path_USB_STICK.

    This function reads the data from the Path_USB_STICK and stores it in global variables.

    Returns:
        int: 1 if the data was read successfully, 0 otherwise.
    """
    global_vars.g_Daten = []
    global_vars.g_LageZuordnung = []
    global_vars.g_PaketPos = []
    global_vars.g_PaketeZuordnung = []
    global_vars.g_Zwischenlagen = []
    global_vars.g_paket_quer = 1
    global_vars.g_CenterOfGravity = [0,0,0]
    
    logger.debug(f"Trying to read file {global_vars.PATH_USB_STICK + global_vars.FILENAME}")
    
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
            
            Check_Einzelpaket_längs_greifen(pl)

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
        logger.error(f"Error reading file {global_vars.FILENAME}")
    return 1

def Check_Einzelpaket_längs_greifen(package_length: float):
    """
    Check if it would be better to use the Einzelpaket längs greifen.

    Pre-sets the Einzelpaket längs greifen checkbox.
    """
    if package_length >= 265:
        global_vars.ui.checkBoxEinzelpaket.setChecked(True)
    else:
        global_vars.ui.checkBoxEinzelpaket.setChecked(False)
 
 
#funktion für den roboter 
def UR_Palette():
    """
    Get the palette dimensions.

    Returns:
        list: The palette dimensions.
    """
    return global_vars.g_PalettenDim
 
def UR_Karton():
    """
    Get the carton dimensions.

    Returns:
        list: The carton dimensions.
    """
    return global_vars.g_PaketDim
 
def UR_Lagen():
    """
    Get the layer types.

    Returns:
        list: The layer types.
    """
    return global_vars.g_LageZuordnung
 
def UR_Zwischenlagen():
    """
    Get the number of use cycles.

    Returns:
        list: The number of use cycles.
    """
    return global_vars.g_Zwischenlagen
 
def UR_PaketPos(Nummer):
    """
    Get the package position.

    Args:
        Nummer (int): The package number.

    Returns:
        list: The package position.
    """
    return global_vars.g_PaketPos[Nummer]
 
def UR_AnzLagen():
    """
    Get the number of layers.

    Returns:
        int: The number of layers.
    """
    return global_vars.g_AnzLagen
 
def UR_AnzPakete():
    """
    Get the number of packages.

    Returns:
        int: The number of packages.
    """
    return global_vars.g_AnzahlPakete
 
def UR_PaketeZuordnung():
    """
    Get the package order.

    Returns:
        list: The package order.
    """
    return global_vars.g_PaketeZuordnung
 
 
#den "center of gravity" messen
def UR_CoG(Masse_Paket,Masse_Greifer,Anzahl_Pakete=1):
    """
    Calculate the center of gravity.

    Args:
        Masse_Paket (float): The mass of the package.
        Masse_Greifer (float): The mass of the carton.
        Anzahl_Pakete (int, optional): The number of packages. Defaults to 1.

    Returns:
        list: The center of gravity.
    """
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

def UR_Paket_hoehe():
    """
    Set the package height.

    Returns:
        int: The package height.
    """
    global_vars.g_PaketDim[2] = int(global_vars.ui.EingabeKartonhoehe.text())
    return global_vars.g_PaketDim[2]

def UR_Startlage():
    """
    Set the start layer.

    Returns:
        int: The start layer.
    """
    global_vars.g_Startlage = int(global_vars.ui.EingabeStartlage.value())
    return global_vars.g_Startlage

def UR_MasseGeschaetzt():
    """
    Set the mass of the carton.

    Returns:
        float: The mass of the carton.
    """ 
    global_vars.g_MassePaket = float(global_vars.ui.EingabeKartonGewicht.text())
    return global_vars.g_MassePaket

def UR_PickOffsetX():
    """
    Set the pick offset in x direction.

    Returns:
        int: The pick offset in x direction.
    """
    #global_vars.ui
    global_vars.g_Pick_Offset_X = int(global_vars.ui.EingabeVerschiebungX.value())
    return global_vars.g_Pick_Offset_X

def UR_PickOffsetY():
    """
    Set the pick offset in y direction.

    Returns:
        int: The pick offset in y direction.
    """
    #global_vars.ui
    global_vars.g_Pick_Offset_Y = int(global_vars.ui.EingabeVerschiebungY.value())
    return global_vars.g_Pick_Offset_Y


def UR_Quergreifen():
    """
    Query the robot.

    Returns:
        bool: True if the robot is queried, False otherwise.
    """
    #global_vars.ui
    logger.debug(f"{global_vars.ui.checkBoxEinzelpaket.isChecked()=}")
    return global_vars.ui.checkBoxEinzelpaket.isChecked()