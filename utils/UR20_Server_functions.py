# implementation of UR20 functions to be called by the server

from PySide6.QtWidgets import QMessageBox, QLabel
from PySide6.QtGui import QPixmap
from . import global_vars
import time
from main import update_status_label
from typing import Literal, cast
from PySide6.QtCore import Qt

def show_scanner_safety_dialog(Bild: QPixmap) -> bool:
    """Show safety confirmation dialog and return whether user confirmed."""
    if global_vars.ui:
        msg = QMessageBox(global_vars.ui.stackedWidget.widget(0))
        msg.setWindowTitle("Sicherheitswarnung")
        msg.setText("Ist der Arbeitsbereich um den Roboter frei?")
        msg.setIcon(QMessageBox.Icon.Warning)
        msg.setStandardButtons(QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
        msg.setDefaultButton(QMessageBox.StandardButton.No)
        
        # Make dialog frameless and always on top
        msg.setWindowFlags(
            Qt.WindowType.FramelessWindowHint | 
            Qt.WindowType.WindowStaysOnTopHint
        )
        
        # Add scanner image
        label = cast(QLabel | None, msg.findChild(QLabel, "", Qt.FindChildOption.FindChildrenRecursively))
        if label:
            label.setPixmap(Bild)
            
        return msg.exec() == QMessageBox.StandardButton.Yes
    return False

def UR20_scannerStatus(status: str) -> int:
    """Set the scanner status."""
    if not status == "True,True,True" and global_vars.timestamp_scanner_fault is None:
        global_vars.timestamp_scanner_fault = time.time()
        update_status_label("Bitte Arbeitsbereich räumen.", "red", True, block=True)

    image: QPixmap | None = None
    match status:
        case "True,True,True":
            image = QPixmap(u':/ScannerUR20/imgs/UR20/scanner1&2&3io.png')
        case "False,False,False":
            image = QPixmap(u':/ScannerUR20/imgs/UR20/scanner1&2&3nio.png')
        case "True,False,False":
            image = QPixmap(u':/ScannerUR20/imgs/UR20/scanner1io.png')
        case "False,True,False":
            image = QPixmap(u':/ScannerUR20/imgs/UR20/scanner2io.png')
        case "False,False,True":
            image = QPixmap(u':/ScannerUR20/imgs/UR20/scanner3io.png')
        case "True,True,False":
            image = QPixmap(u':/ScannerUR20/imgs/UR20/scanner3nio.png')
        case "True,False,True":
            image = QPixmap(u':/ScannerUR20/imgs/UR20/scanner2nio.png')
        case "False,True,True":
            image = QPixmap(u':/ScannerUR20/imgs/UR20/scanner1nio.png')
    if image and global_vars.ui and global_vars.ui.label_7:
        global_vars.ui.label_7.setPixmap(image)
        if not status == "True,True,True":
            if show_scanner_safety_dialog(image):
                if global_vars.message_manager:
                    global_vars.message_manager.unblock_message("Bitte Arbeitsbereich räumen.")
                    global_vars.message_manager.acknowledge_message("Bitte Arbeitsbereich räumen.")
                    global_vars.timestamp_scanner_fault = None
    return 0


# change active pallet
def UR20_SetActivePalette(pallet_number) -> Literal[0] | Literal[1]:
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
def UR20_GetActivePaletteNumber() -> int:
    '''
    returns: 
        current number of active pallet
        where 1 is the first pallet and 2 is the second pallet and 0 is no pallet
    '''
    # TODO: Check to see what pallet is currently active
    return global_vars.UR20_active_palette

# get pallet status
def UR20_GetPaletteStatus(pallet_number) -> Literal[1] | Literal[0] | Literal[-1]:
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