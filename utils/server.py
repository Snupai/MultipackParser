import threading
import socket
from xmlrpc.server import SimpleXMLRPCServer
from typing import Literal

from utils import global_vars
from utils import UR_Common_functions as UR
from utils import UR10_Server_functions as UR10
from utils import UR20_Server_functions as UR20
from utils.message import MessageType
from utils.message_manager import MessageManager
from utils.logging_config import setup_server_logger

logger = setup_server_logger()

def server_start() -> Literal[0]:
    """Start the XMLRPC server.

    Returns:
        Literal[0]: The exit code of the application.
    """
    try:
        # Initialize server if not already initialized
        if global_vars.server is None:
            global_vars.server = SimpleXMLRPCServer(("", 8080), allow_none=True)
            logger.debug("Start Server")
        # Get settings from global vars
        settings = global_vars.settings
        robot_type = settings.settings['info']['UR_Model']
        logger.debug(f"Robot type: {robot_type}")
        if robot_type not in ['UR10', 'UR20']:
            # default to UR10
            robot_type = 'UR10'
            logger.warning(f"Invalid robot type {robot_type}, defaulting to UR10")
    except (AttributeError, KeyError, TypeError) as e:
        # If there's any error accessing the settings, default to UR10
        robot_type = 'UR10'
        logger.error(f"Error accessing robot type from settings: {e}. Defaulting to UR10")
        
    # Wrapper to log function calls
    def log_rpc_call(func, name):
        def wrapper(*args, **kwargs):
            logger.info(f"XMLRPC call: {name} - Args: {args}, Kwargs: {kwargs}")
            result = func(*args, **kwargs)
            logger.info(f"XMLRPC result: {name} -> {result}")
            return result
        return wrapper

    # Register common functions for both robot types
    common_functions = [
        (UR.UR_SetFileName, "UR_SetFileName"),
        (UR.UR_ReadDataFromUsbStick, "UR_ReadDataFromUsbStick"),
        (UR.UR_Palette, "UR_Palette"),
        (UR.UR_Karton, "UR_Karton"),
        (UR.UR_Lagen, "UR_Lagen"),
        (UR.UR_Zwischenlagen, "UR_Zwischenlagen"),
        (UR.UR_PaketPos, "UR_PaketPos"),  # type: ignore
        (UR.UR_AnzLagen, "UR_AnzLagen"),
        (UR.UR_AnzPakete, "UR_AnzPakete"),
        (UR.UR_PaketeZuordnung, "UR_PaketeZuordnung"),
        (UR.UR_Paket_hoehe, "UR_Paket_hoehe"),
        (UR.UR_Startlage, "UR_Startlage"),
        (UR.UR_Quergreifen, "UR_Quergreifen"),
        (UR.UR_CoG, "UR_CoG"),  # type: ignore
        (UR.UR_MasseGeschaetzt, "UR_MasseGeschaetzt"),
        (UR.UR_PickOffsetX, "UR_PickOffsetX"),
        (UR.UR_PickOffsetY, "UR_PickOffsetY")
    ]

    for func, name in common_functions:
        global_vars.server.register_function(log_rpc_call(func, name), name)
    
    
    # Register robot type specific functions here if needed
    if robot_type == 'UR10':
        ur10_functions = [
            (UR10.UR10_scanner1and2niobild, "UR_scanner1and2niobild"),
            (UR10.UR10_scanner1bild, "UR_scanner1bild"),
            (UR10.UR10_scanner2bild, "UR_scanner2bild"),
            (UR10.UR10_scanner1and2iobild, "UR_scanner1and2iobild")
        ]
        for func, name in ur10_functions:
            global_vars.server.register_function(log_rpc_call(func, name), name)
    elif robot_type == 'UR20':
        ur20_functions = [
            (UR20.UR20_scannerStatus, "UR_scannerStatus"),  # type: ignore
            (UR20.UR20_SetActivePalette, "UR_SetActivePalette"),
            (UR20.UR20_GetActivePaletteNumber, "UR_GetActivePaletteNumber"),
            (UR20.UR20_GetPaletteStatus, "UR_GetPaletteStatus"),
            (UR20.UR20_SetZwischenLageLegen, "UR_SetZwischenLageLegen"),
            (UR20.UR20_GetKlemmungAktiv, "UR_GetKlemmungAktiv")
        ]
        for func, name in ur20_functions:
            global_vars.server.register_function(log_rpc_call(func, name), name)

    logger.debug(f"Successfully registered functions for {robot_type}")
    
    global_vars.server.serve_forever()
    return 0

def server_stop() -> None:
    """Stop the XMLRPC server.
    """
    if global_vars.server:
        if global_vars.ui and global_vars.ui.ButtonStopRPCServer:
            global_vars.ui.ButtonStopRPCServer.setEnabled(False)
        try:
            # First shutdown the server
            global_vars.server.shutdown()
            # Close the socket connection
            global_vars.server.server_close()
            # Wait for server thread to complete if it exists
            if hasattr(global_vars, 'server_thread') and global_vars.server_thread:
                global_vars.server_thread.join(timeout=5)  # Wait up to 5 seconds for thread to finish
                if global_vars.server_thread.is_alive():
                    logger.warning("Server thread did not terminate within timeout")
                global_vars.server_thread = None
            # Set server to None to prevent further access
            global_vars.server = None
            logger.debug("Server stopped")
            datensenden_manipulation(True, "Server starten", "")
            if global_vars.message_manager is None:
                global_vars.message_manager = MessageManager()
            message = global_vars.message_manager.add_message("XMLRPC Server gestoppt", MessageType.INFO)
            global_vars.message_manager.acknowledge_message(message)
        except Exception as e:
            logger.error(f"Error stopping server: {e}")

def server_thread() -> None:
    """Start the XMLRPC server in a separate thread.
    """
    logger.debug("Starting server thread")
    # Store thread reference in global_vars
    global_vars.server_thread = threading.Thread(target=server_start)
    global_vars.server_thread.daemon = True  # Make thread daemon so it exits when main thread exits
    global_vars.server_thread.start()
    if global_vars.ui and global_vars.ui.ButtonStopRPCServer:
        global_vars.ui.ButtonStopRPCServer.setEnabled(True)
    datensenden_manipulation(False, "Server läuft", "green")
    if global_vars.message_manager is None:
        global_vars.message_manager = MessageManager()
    message = global_vars.message_manager.add_message("XMLRPC Server gestartet", MessageType.INFO)
    global_vars.message_manager.acknowledge_message(message)

def datensenden_manipulation(visibility: bool, display_text: str, display_colour: str) -> None:
    """Manipulate the visibility of the "Daten Senden" button and the display text.

    Args:
        visibility (bool): Whether the "Daten Senden" button should be visible.
        display_text (str): The text to be displayed in the "Daten Senden" button.
        display_colour (str): The colour of the "Daten Senden" button.
    """
    if global_vars.ui:
        buttons = []
        if hasattr(global_vars.ui, 'ButtonDatenSenden'):
            buttons.append(global_vars.ui.ButtonDatenSenden)
        if hasattr(global_vars.ui, 'ButtonDatenSenden_2'):
            buttons.append(global_vars.ui.ButtonDatenSenden_2)
            
        for button in buttons:
            button.setStyleSheet(f"color: {display_colour}")
            button.setEnabled(visibility)
            button.setText(display_text) 