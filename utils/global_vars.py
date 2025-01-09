from .palletizer import Palletizer

# Create global instance of Palletizer
palletizer = Palletizer()

# For backwards compatibility (can be gradually removed)
logger = palletizer.logger
settings = None
process = None
VERSION = palletizer.version
robot_ip = palletizer.robot_ip
PATH_USB_STICK = palletizer.path_usb_stick
FILENAME = palletizer.filename
g_PalettenDim = palletizer.pallet_dimensions
g_PaketDim  = palletizer.package_dimensions
g_LageArten = palletizer.layer_types
g_Daten = palletizer.raw_data
g_LageZuordnung = palletizer.layer_assignments
g_PaketPos = palletizer.package_positions
g_AnzahlPakete = palletizer.num_packages
g_AnzLagen = palletizer.num_layers
g_PaketeZuordnung = palletizer.package_assignments
g_Zwischenlagen = palletizer.intermediate_layers
g_Startlage = palletizer.start_layer
g_paket_quer = palletizer.package_transverse
g_CenterOfGravity = palletizer.center_of_gravity
g_MassePaket = palletizer.package_mass
g_Pick_Offset_X = palletizer.pick_offset_x
g_Pick_Offset_Y = palletizer.pick_offset_y
ui = palletizer.ui

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