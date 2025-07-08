# implementation of UR20 functions to be called by the server

from PySide6.QtWidgets import QMessageBox, QLabel
from PySide6.QtGui import QPixmap
import utils
from utils.system.core import global_vars
from datetime import datetime
from utils.message.status_manager import update_status_label
from typing import Literal, cast, Union
from PySide6.QtCore import Qt, QObject, Signal, QTimer
import logging
# from utils.audio.audio import kill_play_stepback_warning_thread, spawn_play_stepback_warning_thread
import time
from utils.audio.audio import play_audio, stop_audio

from utils.system.config.logging_config import setup_server_logger

logger = setup_server_logger()

class ScannerSignals(QObject):
    """Signals for the scanner.

    Args:
        QObject (QObject): The parent class of the signals.
    """
    status_changed = Signal(str, str)  # status, image_path

scanner_signals = ScannerSignals()

# Helper function to mark palette as not empty and record timestamp
def mark_palette_not_empty(palette_number: int) -> None:
    """Mark a palette as not empty and record the timestamp.
    
    Args:
        palette_number (int): The palette number (1 or 2)
    """
    logger.info(f"Marking palette {palette_number} as not empty")
    current_time = time.time()
    
    if palette_number == 1:
        global_vars.UR20_palette1_empty = False
        if not hasattr(global_vars, 'palette1_nonempty_timestamp'):
            global_vars.palette1_nonempty_timestamp = current_time
    elif palette_number == 2:
        global_vars.UR20_palette2_empty = False
        if not hasattr(global_vars, 'palette2_nonempty_timestamp'):
            global_vars.palette2_nonempty_timestamp = current_time
    else:
        logger.error(f"Invalid palette number: {palette_number}")

def UR20_scannerStatus(status: str) -> int:
    """Set the scanner status.

    Args:
        status (str): The status of the scanner.

    Returns:
        int: The exit code of the application.
    """
    logger.debug(f"Scanner status update received: {status}")
    
    # Track previous status for audio changes
    previous_status = getattr(global_vars, 'previous_scanner_status', "True,True,True")
    global_vars.previous_scanner_status = status
    
    # Handle scanner fault detection
    if status != "True,True,True":
        if global_vars.timestamp_scanner_fault is None:
            logger.warning("Scanner fault detected")
            global_vars.timestamp_scanner_fault = datetime.now().timestamp()
        
        # Play scanner warning sound if status changed from safe to unsafe
        if previous_status == "True,True,True":
            current_time = time.time()
            # Check if warning sound hasn't been played in the last 15 seconds
            if (global_vars.last_scanner_warning_time is None or 
                current_time - global_vars.last_scanner_warning_time >= 15):
                
                # Get scanner warning sound file from settings
                if hasattr(global_vars, 'settings') and global_vars.settings:
                    scanner_warning_sound = global_vars.settings.settings['admin']['scanner_warning_sound_file']
                    logger.info(f"Scanner status changed to unsafe, playing warning sound: {scanner_warning_sound}")
                    play_audio("scanner_warning", scanner_warning_sound, loop=False)
                    global_vars.last_scanner_warning_time = current_time
                else:
                    logger.warning("Settings not available, cannot play scanner warning sound")
    else:
        # Reset scanner fault timestamp when all scanners are safe
        if global_vars.timestamp_scanner_fault is not None:
            logger.info("Scanner fault cleared")
            global_vars.timestamp_scanner_fault = None
            # Stop any playing warning sound
            # kill_play_stepback_warning_thread()

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
    
    # Mark the active palette as not empty since it will be used
    mark_palette_not_empty(pallet_number)
    
    return global_vars.UR20_active_palette


def UR20_RequestPaletteChange(old_pallet_number: int, new_pallet_number: int) -> int:
    """Set the palette change request.

    Args:
        old_pallet_number (int): The number of the old pallet.
        new_pallet_number (int): The number of the new pallet.

    Returns:
        int: 1 if palette change request is good to go, 0 if not, 404 if invalid palette number.
    """
    logger.info(f"Palette change request received: {old_pallet_number} -> {new_pallet_number}")
    
    # Validate new palette number
    if new_pallet_number not in [1, 2]:
        logger.error(f"Invalid new palette number: {new_pallet_number}")
        return 404
    
    # Only check if the new palette is empty
    new_palette_empty = (global_vars.UR20_palette1_empty if new_pallet_number == 1 
                        else global_vars.UR20_palette2_empty)
    
    # New palette must be empty to be used
    if not new_palette_empty:
        logger.warning(f"New palette {new_pallet_number} is not empty - cannot change to it")
        return 0
    
    # All conditions met, mark new palette as not empty and proceed
    ret = UR20_SetActivePalette(new_pallet_number)
    if ret == 503:
        logger.warning(f"Palette {new_pallet_number} is not empty - cannot change to it")
        return 0
    elif ret == 404:
        logger.error(f"Invalid palette number: {new_pallet_number}")
        return 404
    return 1

# get active pallet number
def UR20_GetActivePaletteNumber() -> int:
    """Get the active pallet number.

    Returns:
        int: The number of the active pallet.
    """
    if global_vars.UR20_active_palette in [1, 2]:
        match global_vars.UR20_active_palette:
            case 1:
                if global_vars.UR20_palette1_empty:
                    logger.warning("Palette 1 is empty - return palette 1")
                    mark_palette_not_empty(global_vars.UR20_active_palette)
                    return global_vars.UR20_active_palette
                else:
                    logger.warning("Palette 1 is not empty - return 0")
                    return 0
            case 2:
                if global_vars.UR20_palette2_empty:
                    logger.warning("Palette 2 is empty - return palette 2")
                    mark_palette_not_empty(global_vars.UR20_active_palette)
                    return global_vars.UR20_active_palette
                else:
                    logger.warning("Palette 2 is not empty - return 0")
                    return 0
    return 0

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

