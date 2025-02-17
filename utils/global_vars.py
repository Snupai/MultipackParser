# Globals

import logging
from logging.handlers import RotatingFileHandler
from typing import Union, Optional, List, TYPE_CHECKING
import multiprocessing
from datetime import datetime

if TYPE_CHECKING:
    from utils.settings import Settings
    from ui_files.ui_main_window import Ui_Form
    from ui_files.BlinkingLabel import BlinkingLabel

# Setup logging
log_formatter: logging.Formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
log_handler: RotatingFileHandler = RotatingFileHandler('app.log', maxBytes=5*1024*1024, backupCount=2)
log_handler.setFormatter(log_formatter)

console_handler: logging.StreamHandler = logging.StreamHandler()
console_handler.setFormatter(log_formatter)

logger: logging.Logger = logging.getLogger('multipack_parser')  # Create a named logger
logger.setLevel(logging.INFO)  # Set default logging level
logger.addHandler(log_handler)
logger.addHandler(console_handler)

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