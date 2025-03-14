import socket
import logging
from utils import global_vars

logger = logging.getLogger(__name__)

def send_cmd_play() -> None:
    """Send a command to the robot to start.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        # Connect the socket to the port where the server is listening
        server_address = (global_vars.robot_ip, 29999)
        logger.debug('connecting to %s port %s' %(server_address))
        sock.connect(server_address)
        
        # Send data
        message = 'play\n'
        logger.debug('sending %s' %(message))
        sock.sendall(message.encode('utf-8'))
        
        # Print any response
        data = sock.recv(4096)
        logger.debug('received %s' %(data))
        
    finally:
        logger.debug('closing socket')
        sock.close()

def send_cmd_pause() -> None:
    """Send a command to the robot to pause.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        # Connect the socket to the port where the server is listening
        server_address = (global_vars.robot_ip, 29999)
        logger.debug('connecting to %s port %s' %(server_address))
        sock.connect(server_address)
        
        # Send data
        message = 'pause\n'
        logger.debug('sending %s' %(message))
        sock.sendall(message.encode('utf-8'))
        
        # Print any response
        data = sock.recv(4096)
        logger.debug('received %s' %(data))
        
    finally:
        logger.debug('closing socket')
        sock.close()

def send_cmd_stop() -> None:
    """Send a command to the robot to stop.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        # Connect the socket to the port where the server is listening
        server_address = (global_vars.robot_ip, 29999)
        logger.debug('connecting to %s port %s' %(server_address))
        sock.connect(server_address)
        
        # Send data
        message = 'stop\n'
        logger.debug('sending %s' %(message))
        sock.sendall(message.encode('utf-8'))
        
        # Print any response
        data = sock.recv(4096)
        logger.debug('received %s' %(data))
        
    finally:
        logger.debug('closing socket')
        sock.close()

def load() -> None:
    """Load the pallet plan from file.
    """
    if not global_vars.ui or not hasattr(global_vars.ui, 'EingabePallettenplan'):
        logger.error("UI not initialized")
        return

    from utils.ui_helpers import update_status_label
    from utils import UR_Common_functions as UR

    # Store the text first, then manually clear focus to avoid keyboard issues
    Artikelnummer = global_vars.ui.EingabePallettenplan.text()
    global_vars.ui.EingabePallettenplan.clearFocus()
    
    UR.UR_SetFileName(Artikelnummer)
    
    errorReadDataFromUsbStick = UR.UR_ReadDataFromUsbStick()
    if errorReadDataFromUsbStick:
        logger.error(f"Error reading file for {Artikelnummer=} no file found")
        update_status_label("Kein Plan gefunden", "red", True)
        return

    # Enable UI elements and update values only if UI exists
    if global_vars.ui:
        logger.debug(f"File for {Artikelnummer=} found")
        if global_vars.message_manager:
            message_strings = ["Kein Pallettenplan geladen", "Kein Plan gefunden"]
            for message_string in message_strings:
                # unblock the message if it is blocked
                if message_string in global_vars.message_manager._blocked_messages:
                    global_vars.message_manager.unblock_message(message_string)
                global_vars.message_manager.acknowledge_message(message_string)
        update_status_label("Plan erfolgreich geladen", "green", instant_acknowledge=True)
        
        # Enable buttons and input fields
        global_vars.ui.ButtonOpenParameterRoboter.setEnabled(True)
        global_vars.ui.ButtonDatenSenden.setEnabled(True)
        global_vars.ui.EingabeKartonGewicht.setEnabled(True)
        global_vars.ui.EingabeKartonhoehe.setEnabled(True)
        global_vars.ui.EingabeStartlage.setEnabled(True)
        global_vars.ui.checkBoxEinzelpaket.setEnabled(True)

        # Update Startlage SpinBox with new max value
        if global_vars.g_AnzLagen is not None:
            global_vars.ui.EingabeStartlage.setMaximum(global_vars.g_AnzLagen)
            # If current value is above new max, it will be automatically clamped

        if global_vars.g_PaketDim is None:
            logger.error("Package dimensions not initialized")
            return

        Volumen = (global_vars.g_PaketDim[0] * global_vars.g_PaketDim[1] * global_vars.g_PaketDim[2]) / 1E+9 # in m³
        logger.debug(f"{Volumen=}")
        Dichte = 1000 # Dichte von Wasser in kg/m³
        logger.debug(f"{Dichte=}")
        Ausnutzung = 0.4 # Empirsch ermittelter Faktor - nicht für Gasflaschen
        logger.debug(f"{Ausnutzung=}")
        Gewicht = round(Volumen * Dichte * Ausnutzung, 1) # Gewicht in kg
        logger.debug(f"{Gewicht=}")
        global_vars.ui.EingabeKartonGewicht.setText(str(Gewicht))
        global_vars.ui.EingabeKartonhoehe.setText(str(global_vars.g_PaketDim[2]))

def load_wordlist() -> list:
    """Load the wordlist from the USB stick.

    Returns:
        list: A list of wordlist items.
    """
    import os
    wordlist = []
    count = 0
    for file in os.listdir(global_vars.PATH_USB_STICK):
        if file.endswith(".rob"):
            wordlist.append(file[:-4])
            count = count + 1
    logger.debug(f"Wordlist {count=}")
    if hasattr(global_vars, 'settings'):
        global_vars.settings.settings['info']['number_of_plans'] = count
    return wordlist

def load_rob_files():
    """Load .rob files into the list widget."""
    import os
    if not global_vars.ui:
        return
        
    global_vars.ui.robFilesListWidget.clear()
    rob_files = []
    for file in os.listdir(global_vars.PATH_USB_STICK):
        if file.endswith(".rob"):
            rob_files.append(file[:-4])
    
    # Sort the list alphabetically
    rob_files.sort()
    
    # Add sorted items to the list widget
    for file in rob_files:
        global_vars.ui.robFilesListWidget.addItem(file)

def display_selected_file(item):
    """Display the selected file in 3D.

    Args:
        item (QListWidgetItem): The selected item.
    """
    if not global_vars.ui:
        return
    
    from ui_files.visualization_3d import MatplotlibCanvas, display_pallet_3d
    
    # Get the canvas from the frame
    canvas = global_vars.ui.MatplotLibCanvasFrame.findChild(MatplotlibCanvas)
    if not canvas:
        return
        
    # Display the pallet in 3D
    display_pallet_3d(canvas, item.text()) 