# Globals

import logging
from logging.handlers import RotatingFileHandler

# Setup logging
log_formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
log_handler = RotatingFileHandler('app.log', maxBytes=5*1024*1024, backupCount=2)
log_handler.setFormatter(log_formatter)

console_handler = logging.StreamHandler()
console_handler.setFormatter(log_formatter)

logger = logging.getLogger('multipack_parser')  # Create a named logger
logger.setLevel(logging.INFO)  # Set default logging level
logger.addHandler(log_handler)
logger.addHandler(console_handler)

process = None

#######################################
# Settings

settings = None

# Variables

settings_file = 'MultipackParser.conf'

VERSION = '1.5.2-beta'

robot_ip = '192.168.0.1' # DO NOT CHANGE

PATH_USB_STICK = '..'
FILENAME = None
g_PalettenDim = None
g_PaketDim  = None
g_LageArten = None
g_Daten = None
g_LageZuordnung = None
g_PaketPos = None
g_AnzahlPakete = None
g_AnzLagen = None
g_PaketeZuordnung = None
g_Zwischenlagen = None
g_Startlage = None
g_paket_quer = None
g_CenterOfGravity = None
g_MassePaket = None
g_Pick_Offset_X = None
g_Pick_Offset_Y = None
ui = None

# Konstanten für Datenstruktur
#List Index
LI_PALETTE_DATA = 0
LI_PACKAGE_DATA = 1
LI_LAYERTYPES = 2
LI_NUMBER_OF_LAYERS = 3
#Palette Values
LI_PALETTE_DATA_LENGTH = 0
LI_PALETTE_DATA_WIDTH = 1
LI_PALETTE_DATA_HEIGHT = 2
#Package Values
LI_PACKAGE_DATA_LENGTH = 0
LI_PACKAGE_DATA_WIDTH = 1
LI_PACKAGE_DATA_HEIGHT = 2
LI_PACKAGE_DATA_GAP = 3
#Position Values
LI_POSITION_XP = 0
LI_POSITION_YP = 1
LI_POSITION_AP = 2
LI_POSITION_XD = 3
LI_POSITION_YD = 4
LI_POSITION_AD = 5
LI_POSITION_NOP = 6
LI_POSITION_XVEC = 7
LI_POSITION_YVEC = 8
#Number of Entries
NOE_PALETTE_VALUES = 3
NOE_PACKAGE_VALUES = 4
NOE_LAYERTYPES_VALUES = 1
NOE_NUMBER_OF_LAYERS = 1
NOE_PACKAGE_PER_LAYER = 1
NOE_PACKAGE_POSITION_INFO = 9