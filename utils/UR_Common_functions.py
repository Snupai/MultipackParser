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
    """Set the filename."""
    return global_vars.palletizer.set_filename(Artikelnummer)
 
 
#daten vom usb stick hochladen und lesbar machen 
def UR_ReadDataFromUsbStick():
    """Read data from the USB stick."""
    return global_vars.palletizer.read_data_from_usb()
 
 
#funktion für den roboter 
def UR_Palette():
    """Get the palette dimensions."""
    dims = global_vars.palletizer.pallet_dimensions
    return [dims.length, dims.width, dims.height] if dims else None
 
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
 
    
#die funktion für den audio file zu spielen
def UR_StepBack():
    """
    Play the stepback sound.
    """
    file = AudioSegment.from_file(file=u':/Sound/imgs/stepback.mp3', format="mp3")
    play(file)
    return


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