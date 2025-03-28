from . import global_vars
from PySide6.QtGui import QPixmap
from utils.logging_config import setup_server_logger

logger = setup_server_logger()

def UR10_scanner1and2niobild() -> None:
    """Set the scanner image.
    """
    if global_vars.ui and global_vars.ui.label_7:
        global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR10e/imgs/scanner1&2nio.png'))
    return

def UR10_scanner1bild() -> None:
    """Set the scanner image.
    """
    if global_vars.ui and global_vars.ui.label_7:
        global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR10e/imgs/scanner1nio.png'))
    return

def UR10_scanner2bild() -> None:
    """Set the scanner image.
    """
    if global_vars.ui and global_vars.ui.label_7:
        global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR10e/imgs/scanner2nio.png'))
    return

def UR10_scanner1and2iobild() -> None:
    """Set the scanner image.
    """
    if global_vars.ui and global_vars.ui.label_7:
        global_vars.ui.label_7.setPixmap(QPixmap(u':/ScannerUR10e/imgs/scannerio.png'))
    return
