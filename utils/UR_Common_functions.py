# port all functions from main.py that start with UR_ to this file and then import them into main.py to clean up the code
# This file will contain all the functions that are used to communicate with the UR10 robot
# This file will be imported into main.py to clean up the code

from typing import Literal, List, Optional, Union, Tuple

from utils.database import load_from_database
from . import global_vars

from utils.logging_config import setup_server_logger

logger = setup_server_logger()

#Dateiname abfragen
def UR_SetFileName(Artikelnummer) -> str:
    """Set the filename.

    Args:
        Artikelnummer (str): The article number.

    Returns:
        str: The filename.
    """
    global_vars.FILENAME = (Artikelnummer + '.rob')
    logger.debug(f"{global_vars.FILENAME=}")
    return global_vars.FILENAME 
 
 
#daten vom usb stick hochladen und lesbar machen 
def UR_ReadDataFromUsbStick() -> Union[Literal[0], Literal[1]]:
    """Read data from the Path_USB_STICK.

    Returns:
        Union[Literal[0], Literal[1]]: 1 if the data was read successfully, 0 otherwise.
    """
    
    global_vars.g_Daten = []
    global_vars.g_LageZuordnung = []
    global_vars.g_PaketPos = []
    global_vars.g_PaketeZuordnung = []
    global_vars.g_Zwischenlagen = []
    global_vars.g_paket_quer = 1
    global_vars.g_CenterOfGravity = [0,0,0]
    
    if global_vars.FILENAME is None:
        logger.error("No filename set")
        return 0
    logger.debug(f"Trying to read file {global_vars.FILENAME}")
    
    try:
        global_vars.g_Daten, *_ = load_from_database(file_name=global_vars.FILENAME)
    
    
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

def Check_Einzelpaket_längs_greifen(package_length: int) -> None:
    """Check if package should be gripped lengthwise.
    """
    if global_vars.ui and global_vars.ui.checkBoxEinzelpaket:
        if package_length >= 265:
            global_vars.ui.checkBoxEinzelpaket.setChecked(True)
        else:
            global_vars.ui.checkBoxEinzelpaket.setChecked(False)
 
 
#funktion für den roboter 
def UR_Palette() -> Optional[List[int]]:
    """Get the palette dimensions.

    Returns:
        Optional[List[int]]: The palette dimensions, or None if not available.
    """
    return global_vars.g_PalettenDim
 
def UR_Karton() -> Optional[List[int]]:
    """Get the carton dimensions.

    Returns:
        Optional[List[int]]: The carton dimensions, or None if not available.
    """
    return global_vars.g_PaketDim
 
def UR_Lagen() -> Optional[List[int]]:
    """Get the layer types.

    Returns:
        Optional[List[int]]: The layer types, or None if not available.
    """
    return global_vars.g_LageZuordnung
 
def UR_Zwischenlagen() -> Optional[List[int]]:
    """Get the number of use cycles.

    Returns:
        Optional[List[int]]: The number of use cycles, or None if not available.
    """
    return global_vars.g_Zwischenlagen
 
def UR_PaketPos(Nummer: int) -> Optional[List[int]]:
    """Get the package position, with coordinate transformation for palette 2.

    Args:
        Nummer (int): The package number.

    Returns:
        Optional[List[int]]: The package position, or None if not available.
    """
    if global_vars.g_PaketPos is None:
        logger.error("Package positions not initialized")
        return None
        
    pos = global_vars.g_PaketPos[Nummer]
    px, py, pr = pos[0], pos[1], pos[2]
    x, y, r = pos[3], pos[4], pos[5]
    n = pos[6]
    dx, dy = pos[7], pos[8]
    if global_vars.ui.checkBoxLabelInvert.isChecked():
        r = (r + 180) % 360
    if global_vars.UR20_active_palette == 2:
        # For palette 2, transform coordinates using:
        # (px, py, pr, x, y, r, n, dx, dy) -> (px, py, pr, y, x, (r+180)mod360, n, dy, dx)
        
        # Apply transformation
        temp_x = x
        x = y
        y = temp_x
        r = (r + 180) % 360
        temp_dx = dx
        dx = dy
        dy = temp_dx
        
    return [px, py, pr, x, y, r, n, dx, dy]

def UR_AnzLagen() -> Optional[int]:
    """Get the number of layers.

    Returns:
        Optional[int]: The number of layers, or None if not available.
    """
    return global_vars.g_AnzLagen
 
def UR_AnzPakete() -> Optional[int]:
    """Get the number of packages.

    Returns:
        Optional[int]: The number of packages, or None if not available.
    """
    return global_vars.g_AnzahlPakete
 
def UR_PaketeZuordnung() -> Optional[List[int]]:
    """Get the package order.

    Returns:
        Optional[List[int]]: The package order, or None if not available.
    """
    return global_vars.g_PaketeZuordnung
 
 
#den "center of gravity" messen
def UR_CoG(Masse_Paket: float, Masse_Greifer: float, Anzahl_Pakete: int = 1) -> Optional[List[float]]:
    """Calculate the center of gravity.

    Args:
        Masse_Paket (float): The mass of the package.
        Masse_Greifer (float): The mass of the carton.
        Anzahl_Pakete (int, optional): The number of packages. Defaults to 1.

    Returns:
        Optional[List[float]]: The center of gravity, or None if it couldn't be calculated.
    """
    if global_vars.g_PaketDim is None:
        logger.error("Package dimensions not initialized")
        return None
        
    # Initialize g_CenterOfGravity if None
    if global_vars.g_CenterOfGravity is None:
        global_vars.g_CenterOfGravity = [0.0, 0.0, 0.0]
        
    if Anzahl_Pakete == 0:
        Masse_Paket = 0
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

def UR_Paket_hoehe() -> int:
    """Set the package height.

    Returns:
        int: The package height.
    """
    if global_vars.g_PaketDim is None:
        logger.error("Package dimensions not initialized")
        return 0
        
    if global_vars.ui and global_vars.ui.EingabeKartonhoehe:
        global_vars.g_PaketDim[2] = int(global_vars.ui.EingabeKartonhoehe.text())
        return global_vars.g_PaketDim[2]
    return 0

def UR_Startlage() -> int:
    """Set the start layer.

    Returns:
        int: The start layer.
    """
    if global_vars.ui and global_vars.ui.EingabeStartlage:
        global_vars.g_Startlage = int(global_vars.ui.EingabeStartlage.value())
        return global_vars.g_Startlage
    return 0

def UR_MasseGeschaetzt() -> float:
    """Set the mass of the carton.

    Returns:
        float: The mass of the carton.
    """
    if global_vars.ui and global_vars.ui.EingabeKartonGewicht:
        global_vars.g_MassePaket = float(global_vars.ui.EingabeKartonGewicht.text())
        return global_vars.g_MassePaket
    return 0.0

def UR_PickOffsetX() -> int:
    """Set the pick offset in x direction.

    Returns:
        int: The pick offset in x direction.
    """
    if global_vars.ui and global_vars.ui.EingabeVerschiebungX:
        global_vars.g_Pick_Offset_X = int(global_vars.ui.EingabeVerschiebungX.value())
        return global_vars.g_Pick_Offset_X
    return 0

def UR_PickOffsetY() -> int:
    """Set the pick offset in y direction.

    Returns:
        int: The pick offset in y direction.
    """
    if global_vars.ui and global_vars.ui.EingabeVerschiebungY:
        global_vars.g_Pick_Offset_Y = int(global_vars.ui.EingabeVerschiebungY.value())
        return global_vars.g_Pick_Offset_Y
    return 0

def UR_Quergreifen() -> bool:
    """Query the robot.

    Returns:
        bool: True if the robot is queried, False otherwise.
    """
    if global_vars.ui and global_vars.ui.checkBoxEinzelpaket:
        logger.debug(f"{global_vars.ui.checkBoxEinzelpaket.isChecked()=}")
        return global_vars.ui.checkBoxEinzelpaket.isChecked()
    return False