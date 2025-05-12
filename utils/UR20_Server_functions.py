# implementation of UR20 functions to be called by the server

from PySide6.QtWidgets import QMessageBox, QLabel
from PySide6.QtGui import QPixmap
from . import global_vars
from datetime import datetime
from utils.status_manager import update_status_label
from typing import Literal, cast, Union
from PySide6.QtCore import Qt, QObject, Signal, QTimer
import logging
from utils.audio import kill_play_stepback_warning_thread, spawn_play_stepback_warning_thread

from utils.logging_config import setup_server_logger

logger = setup_server_logger()

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
    logger.debug(f"Scanner status update received: {status}")
    
    # Handle scanner fault detection
    if status != "True,True,True":
        if global_vars.timestamp_scanner_fault is None:
            logger.warning("Scanner fault detected")
            global_vars.timestamp_scanner_fault = datetime.now().timestamp()
    else:
        # Reset scanner fault timestamp when all scanners are safe
        if global_vars.timestamp_scanner_fault is not None:
            logger.info("Scanner fault cleared")
            global_vars.timestamp_scanner_fault = None
            # Stop any playing warning sound
            kill_play_stepback_warning_thread()

    image_path = None
    message = "Bitte Arbeitsbereich räumen."  # Default message for unsafe conditions
    
    match status:
        case "True,True,True":
            logger.info("All scanners report safe conditions")
            image_path = u':/ScannerUR20/imgs/UR20/scanner1&2&3io.png'
            message = "Alles in Ordnung"
        case "False,False,False":
            logger.warning("All scanners report unsafe conditions")
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
        logger.debug(f"Updating scanner image: {image_path}")
        try:
            if global_vars.ui and global_vars.ui.label_7:
                global_vars.ui.label_7.setPixmap(QPixmap(image_path))
            scanner_signals.status_changed.emit(status, image_path)
        except Exception as e:
            logger.error(f"Failed to update scanner image: {e}")
    return 0

# change active pallet
def UR20_SetActivePalette(pallet_number) -> Union[Literal[0], Literal[1]]:
    """Set the active pallet.

    Args:
        pallet_number (int): The number of the pallet to be set as active.

    Returns:
        Literal[0] | Literal[1]: 0 if the pallet is not empty, 1 if the pallet is empty.
    """
    logger.debug(f"Request to set active palette to {pallet_number}")
    
    if pallet_number == 1 and not global_vars.UR20_palette1_empty:
        logger.warning("Cannot set palette 1 as active - not empty")
        return 503
    elif pallet_number == 2 and not global_vars.UR20_palette2_empty:
        logger.warning("Cannot set palette 2 as active - not empty")
        return 503
    elif pallet_number not in [1, 2]:
        logger.error(f"Invalid palette number: {pallet_number}")
        return 404
        
    logger.info(f"Setting active palette to {pallet_number}")
    global_vars.UR20_active_palette = pallet_number
    return global_vars.UR20_active_palette

# get active pallet number
def UR20_GetActivePaletteNumber() -> int:
    """Get the active pallet number.

    Returns:
        int: The number of the active pallet.
    """
    logger.debug(f"Current active palette: {global_vars.UR20_active_palette}")
    return global_vars.UR20_active_palette

# get pallet status
def UR20_GetPaletteStatus(pallet_number) -> Union[Literal[1], Literal[0], Literal[-1]]:
    """Get the status of the given pallet.

    Returns:
        Union[Literal[1], Literal[0], Literal[-1]]: 1 if the pallet is empty, 0 if the pallet is full, -1 if the pallet number is invalid.
    """
    logger.debug(f"Checking status of palette {pallet_number}")
    
    match pallet_number:
        case 1:
            status = 1 if global_vars.UR20_palette1_empty else 0
            logger.info(f"Palette 1 status: {'empty' if status else 'not empty'}")
            return status
        case 2:
            status = 1 if global_vars.UR20_palette2_empty else 0
            logger.info(f"Palette 2 status: {'empty' if status else 'not empty'}")
            return status
        case _:
            logger.error(f"Invalid palette number: {pallet_number}")
            return -1
        
def UR20_SetZwischenLageLegen(aktiv: bool):
    """Set the zwischenlage.

    Args:
        aktiv (bool): Zwischenlage aktiv legen

    Returns:
        1: Zwischenlage aktiv legen
    """
    logger.info(f"Setting zwischenlage to {aktiv}")
    global_vars.UR20_zwischenlage = bool(aktiv)  # Ensure it's a boolean value
    
    # Log additional info for UI state
    if aktiv:
        logger.warning("Zwischenlage legen und mit Reset bestätigen.")
    else:
        logger.info("Zwischenlage reset confirmed.")
        
    return 1

def UR20_GetKlemmungAktiv() -> bool:
    """Check if the klemmung is active.

    Returns:
        bool: True if the klemmung is active, False otherwise.
    """
    return global_vars.ui.checkBoxKlemmung.isChecked()

def UR20_GetScannerOverride() -> list[bool]:
    """Get the scanner override.

    Returns:
        list[bool]: The status of the scanner override.
    """
    scanner_override: list[bool] = [global_vars.ui.checkBoxScanner1Overwrite.isChecked(), global_vars.ui.checkBoxScanner2Overwrite.isChecked(), global_vars.ui.checkBoxScanner3Overwrite.isChecked()]
    logger.debug(f"Checking scanner override: {scanner_override=}")
    return scanner_override

