# implementation of UR20 functions to be called by the server

from PySide6.QtWidgets import QMessageBox, QLabel
from PySide6.QtGui import QPixmap
from . import global_vars
import time
from main import update_status_label
from typing import Literal, cast
from PySide6.QtCore import Qt, QObject, Signal

class ScannerSignals(QObject):
    """Signals for the scanner.

    Args:
        QObject (QObject): The parent class of the signals.
    """
    status_changed = Signal(str, str)  # status, image_path

scanner_signals = ScannerSignals()

def UR20_scannerStatus(status: str) -> int:
    """Set the scanner status.

    Args:
        status (str): The status of the scanner.

    Returns:
        int: The exit code of the application.
    """
    if not status == "True,True,True" and global_vars.timestamp_scanner_fault is None:
        global_vars.timestamp_scanner_fault = time.time()

    image_path = None
    message = "Bitte Arbeitsbereich räumen."  # Default message for unsafe conditions
    
    match status:
        case "True,True,True":
            image_path = u':/ScannerUR20/imgs/UR20/scanner1&2&3io.png'
            message = "Alles in Ordnung"
        case "False,False,False":
            image_path = u':/ScannerUR20/imgs/UR20/scanner1&2&3nio.png'
        case "True,False,False":
            image_path = u':/ScannerUR20/imgs/UR20/scanner1io.png'
        case "False,True,False":
            image_path = u':/ScannerUR20/imgs/UR20/scanner2io.png'
        case "False,False,True":
            image_path = u':/ScannerUR20/imgs/UR20/scanner3io.png'
        case "True,True,False":
            image_path = u':/ScannerUR20/imgs/UR20/scanner3nio.png'
        case "True,False,True":
            image_path = u':/ScannerUR20/imgs/UR20/scanner2nio.png'
        case "False,True,True":
            image_path = u':/ScannerUR20/imgs/UR20/scanner1nio.png'

    if image_path:
        # Update image directly first
        if global_vars.ui and global_vars.ui.label_7:
            global_vars.ui.label_7.setPixmap(QPixmap(image_path))
        # Then emit signal for other handlers
        scanner_signals.status_changed.emit(status, image_path)
    return 0


# change active pallet
def UR20_SetActivePalette(pallet_number) -> Literal[0] | Literal[1]:
    """Set the active pallet.

    Args:
        pallet_number (int): The number of the pallet to be set as active.

    Returns:
        Literal[0] | Literal[1]: 0 if the pallet is not empty, 1 if the pallet is empty.
    """
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
def UR20_GetActivePaletteNumber() -> int:
    """Get the active pallet number.

    Returns:
        int: The number of the active pallet.
    """
    # TODO: Check to see what pallet is currently active
    return global_vars.UR20_active_palette

# get pallet status
def UR20_GetPaletteStatus(pallet_number) -> Literal[1] | Literal[0] | Literal[-1]:
    """Get the status of the given pallet.

    Returns:
        Literal[1] | Literal[0] | Literal[-1]: 1 if the pallet is empty, 0 if the pallet is full, -1 if the pallet number is invalid.
    """
    # TODO: check if the given pallet number is valid and if the according pallet space is empty or not
    match pallet_number:
        case 1:
            return 1 if global_vars.UR20_palette1_empty else 0
        case 2:
            return 1 if global_vars.UR20_palette2_empty else 0
        case _:
            return -1