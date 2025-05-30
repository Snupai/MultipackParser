from utils import global_vars
from PySide6.QtGui import QPixmap
from utils.system.config.logging_config import setup_server_logger

logger = setup_server_logger()

def UR10_scanner1and2niobild() -> int:
    """Get the scanner status for both scanners.

    Returns:
        int: The scanner status.
    """
    if global_vars.ui and global_vars.ui.scanner1and2niobild:
        return global_vars.ui.scanner1and2niobild.value()
    return 0

def UR10_scanner1bild() -> int:
    """Get the scanner status for scanner 1.

    Returns:
        int: The scanner status.
    """
    if global_vars.ui and global_vars.ui.scanner1bild:
        return global_vars.ui.scanner1bild.value()
    return 0

def UR10_scanner2bild() -> int:
    """Get the scanner status for scanner 2.

    Returns:
        int: The scanner status.
    """
    if global_vars.ui and global_vars.ui.scanner2bild:
        return global_vars.ui.scanner2bild.value()
    return 0

def UR10_scanner1and2iobild() -> int:
    """Get the scanner status for both scanners.

    Returns:
        int: The scanner status.
    """
    if global_vars.ui and global_vars.ui.scanner1and2iobild:
        return global_vars.ui.scanner1and2iobild.value()
    return 0


