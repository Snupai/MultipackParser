# Globals

import logging
from logging.handlers import RotatingFileHandler
import threading
from typing import Union, Optional, List, TYPE_CHECKING
import multiprocessing
from datetime import datetime
import os
import sys

if TYPE_CHECKING:
    from utils.system.config.settings import Settings
    from ui_files.ui_main_window import Ui_Form
    from ui_files.BlinkingLabel import BlinkingLabel
    from .message.message_manager import MessageManager
    from utils.robot.robot_status_monitor import RobotStatus

from .system.config.logging_config import logger
from .robot.robot_enums import RobotMode, SafetyStatus, ProgramState

# Global variables
process: Optional[multiprocessing.Process] = None
settings: Optional['Settings'] = None
message_manager: Optional['MessageManager'] = None
main_window = None
canvas = None
allow_close = False

# Variables
VERSION: str = '1.5.13'

# Network settings
robot_ip: str = '192.168.0.1'  # DO NOT CHANGE

# Robot Status Variables
current_robot_mode: RobotMode = RobotMode.UNKNOWN
current_safety_status: SafetyStatus = SafetyStatus.UNKNOWN
current_program_state: ProgramState = ProgramState.UNKNOWN
robot_status_monitor: Optional['RobotStatus'] = None

# UR20 palette place
UR20_active_palette: int = 0
UR20_palette1_empty: bool = False
UR20_palette2_empty: bool = False

# UR20 zwischenlage
UR20_zwischenlage: Optional[bool] = False

# Audio
audio_muted: bool = False

timestamp_scanner_fault: Optional[float] = None

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

# Initialize logger
logger.info("Global variables module initialized")

# Filter dimensions for palette list
filter_length: int = 0
filter_width: int = 0
filter_height: int = 0

audio_thread: Optional[threading.Thread] = None

# XMLRPC Server instance
server = None
