# implementation of UR20 functions to be called by the server

from PySide6.QtGui import QPixmap
from . import global_vars

# the scanner image stuff for 3 scanners
def UR20_scannerStatus(status: str):
    """
    Set the scanner status.
    """

    match status:
        case "True,True,True":
            global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR20/imgs/scanner1&2&3io.png'))
        case "False,False,False":
            global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR20/imgs/scanner1&2&3nio.png'))
        case "True,False,False":
            global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR20/imgs/scanner1io.png'))
        case "False,True,False":
            global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR20/imgs/scanner2io.png'))
        case "False,False,True":
            global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR20/imgs/scanner3io.png'))
        case "True,True,False":
            global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR20/imgs/scanner3nio.png'))
        case "True,False,True":
            global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR20/imgs/scanner2nio.png'))
        case "False,True,True":
            global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR20/imgs/scanner1nio.png'))
    return


# change active pallet
def UR20_SetActivePalette(pallet_number):
    '''
    pallet_number: number of pallet
    
    returns: 
        1 if pallet was set
        0 if pallet was not set
    '''
    return

# get active pallet number
def UR20_GetActivePaletteNumber():
    '''
    returns: current number of active pallet
    '''
    return

# get pallet status
def UR20_GetPaletteStatus(pallet_number):
    '''
    pallet_number: number of pallet

    returns: 
        1 if pallet is empty
        0 if pallet is full
    '''
    return