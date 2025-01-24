# implementation of UR20 functions to be called by the server

from PySide6.QtGui import QPixmap
from . import global_vars
import time
from main import update_status_label

# the scanner image stuff for 3 scanners
def UR20_scannerStatus(status: str):
    """
    Set the scanner status.
    """

    if not status == "True,True,True" and global_vars.timestamp_scanner_fault is None:
        global_vars.timestamp_scanner_fault = time.time()
        update_status_label("Bitte Arbeitsbereich räumen.", "red", True)

    match status:
        case "True,True,True":
            global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR20/imgs/UR20/scanner1&2&3io.png'))
        case "False,False,False":
            global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR20/imgs/UR20/scanner1&2&3nio.png'))
        case "True,False,False":
            global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR20/imgs/UR20/scanner1io.png'))
        case "False,True,False":
            global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR20/imgs/UR20/scanner2io.png'))
        case "False,False,True":
            global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR20/imgs/UR20/scanner3io.png'))
        case "True,True,False":
            global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR20/imgs/UR20/scanner3nio.png'))
        case "True,False,True":
            global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR20/imgs/UR20/scanner2nio.png'))
        case "False,True,True":
            global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR20/imgs/UR20/scanner1nio.png'))
    return


# change active pallet
def UR20_SetActivePalette(pallet_number):
    '''
    pallet_number: number of pallet
    
    returns: 
        1 if pallet was set
        0 if pallet was not set
    '''
    # TODO: Set the active pallet to the given pallet number
    # Check if requested pallet is empty before setting it
    if pallet_number == 1 and not global_vars.UR20_palette1_empty:
        return 0
    elif pallet_number == 2 and not global_vars.UR20_palette2_empty:
        return 0
    elif pallet_number not in [1, 2]:
        return 0
    global_vars.UR20_active_palette = pallet_number
    return 1

# get active pallet number
def UR20_GetActivePaletteNumber():
    '''
    returns: 
        current number of active pallet
        where 1 is the first pallet and 2 is the second pallet and 0 is no pallet
    '''
    # TODO: Check to see what pallet is currently active
    return global_vars.UR20_active_palette

# get pallet status
def UR20_GetPaletteStatus(pallet_number):
    '''
    pallet_number: number of pallet

    returns: 
        1 if pallet is empty
        0 if pallet is full
        -1 if pallet number is invalid
    '''
    # TODO: check if the given pallet number is valid and if the according pallet space is empty or not
    match pallet_number:
        case 1:
            return 1 if global_vars.UR20_palette1_empty else 0
        case 2:
            return 1 if global_vars.UR20_palette2_empty else 0
        case _:
            return -1