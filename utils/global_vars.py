# Globals

import logging
from logging.handlers import RotatingFileHandler
from typing import Union, Optional, List, TYPE_CHECKING
import multiprocessing
from datetime import datetime
import os
import sys
from utils.message_manager import MessageManager

if TYPE_CHECKING:
    from utils.settings import Settings
    from ui_files.ui_main_window import Ui_Form
    from ui_files.BlinkingLabel import BlinkingLabel

def get_log_path() -> str:
    """Get the appropriate log file path whether running as script or frozen exe"""
    if getattr(sys, 'frozen', False):
        # Running as compiled executable
        base_path = os.path.dirname(sys.executable)
    else:
        # Running as script
        base_path = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    
    # Create logs directory if it doesn't exist
    logs_dir = os.path.join(base_path, 'logs')
    os.makedirs(logs_dir, exist_ok=True)
    
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_file = os.path.join(logs_dir, f'multipack_parser_{timestamp}.log')
    
    # Ensure the file can be created
    try:
        with open(log_file, 'a') as f:
            pass
    except Exception as e:
        print(f"Error creating log file: {e}")
        # Fallback to a location we know we can write to
        log_file = os.path.join(os.path.expanduser('~'), f'multipack_parser_{timestamp}.log')
    
    return log_file

def setup_logger() -> logging.Logger:
    """Setup and configure the logger

    Returns:
        logging.Logger: Configured logger instance
    """
    # Create logger
    logger = logging.getLogger('multipack_parser')
    logger.setLevel(logging.INFO)  # Set level first
    
    # Remove any existing handlers
    logger.handlers.clear()
    
    try:
        # Create formatters and handlers
        log_formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
        
        # File handler with timestamp in filename
        log_path = get_log_path()
        file_handler = RotatingFileHandler(
            log_path,
            maxBytes=5*1024*1024,  # 5MB
            backupCount=5,  # Keep 5 backup files
            encoding='utf-8'
        )
        file_handler.setFormatter(log_formatter)
        file_handler.setLevel(logging.INFO)
        logger.addHandler(file_handler)
        
        # Console handler
        console_handler = logging.StreamHandler(sys.stdout)  # Explicitly use stdout
        console_handler.setFormatter(log_formatter)
        console_handler.setLevel(logging.INFO)
        logger.addHandler(console_handler)
        
        # Test the logger
        logger.info(f"Logging initialized. Log file: {log_path}")
        
    except Exception as e:
        print(f"Error setting up logger: {e}")
        # Set up a basic console logger as fallback
        console_handler = logging.StreamHandler(sys.stdout)
        console_handler.setFormatter(logging.Formatter('%(asctime)s - %(levelname)s - %(message)s'))
        logger.addHandler(console_handler)
        logger.error(f"Failed to initialize file logging: {e}")
    
    return logger

# Initialize logger
logger = setup_logger()

# Test the logger immediately
logger.info("Global variables module initialized")

process: Optional[multiprocessing.Process] = None

#######################################
# Settings

settings: Optional['Settings'] = None

# Variables

settings_file: str = 'MultipackParser.conf'

VERSION: str = '1.5.7'

robot_ip: str = '192.168.0.1' # DO NOT CHANGE

# UR20 palette place
UR20_active_palette: int = 0
UR20_palette1_empty: bool = False
UR20_palette2_empty: bool = False

# Audio
audio_muted: bool = False

timestamp_scanner_fault: Optional[datetime] = None

##########################

PATH_USB_STICK: str = '..'
FILENAME: Optional[str] = None
g_PalettenDim: Optional[List[int]] = None
g_PaketDim: Optional[List[int]] = None
g_LageArten: Optional[int] = None
g_Daten: Optional[List[List[int]]] = None
g_LageZuordnung: Optional[List[int]] = None
g_PaketPos: Optional[List[List[int]]] = None
g_AnzahlPakete: Optional[int] = None
g_AnzLagen: Optional[int] = None
g_PaketeZuordnung: Optional[List[int]] = None
g_Zwischenlagen: Optional[List[int]] = None
g_Startlage: Optional[List[int]] = None
g_paket_quer: Optional[int] = None
g_CenterOfGravity: Optional[List[float]] = None
g_MassePaket: Optional[float] = None
g_Pick_Offset_X: Optional[float] = None
g_Pick_Offset_Y: Optional[float] = None
ui: Optional['Ui_Form'] = None

# Konstanten für Datenstruktur
#List Index
LI_PALETTE_DATA: int = 0
LI_PACKAGE_DATA: int = 1
LI_LAYERTYPES: int = 2
LI_NUMBER_OF_LAYERS: int = 3
#Palette Values
LI_PALETTE_DATA_LENGTH: int = 0
LI_PALETTE_DATA_WIDTH: int = 1
LI_PALETTE_DATA_HEIGHT: int = 2
#Package Values
LI_PACKAGE_DATA_LENGTH: int = 0
LI_PACKAGE_DATA_WIDTH: int = 1
LI_PACKAGE_DATA_HEIGHT: int = 2
LI_PACKAGE_DATA_GAP: int = 3
#Position Values
LI_POSITION_XP: int = 0
LI_POSITION_YP: int = 1
LI_POSITION_AP: int = 2
LI_POSITION_XD: int = 3
LI_POSITION_YD: int = 4
LI_POSITION_AD: int = 5
LI_POSITION_NOP: int = 6
LI_POSITION_XVEC: int = 7
LI_POSITION_YVEC: int = 8
#Number of Entries
NOE_PALETTE_VALUES: int = 3
NOE_PACKAGE_VALUES: int = 4
NOE_LAYERTYPES_VALUES: int = 1
NOE_NUMBER_OF_LAYERS: int = 1
NOE_PACKAGE_PER_LAYER: int = 1
NOE_PACKAGE_POSITION_INFO: int = 9

blinking_label: Optional['BlinkingLabel'] = None  # Initialize as None, will be set to BlinkingLabel instance later

message_manager: Optional[MessageManager] = None