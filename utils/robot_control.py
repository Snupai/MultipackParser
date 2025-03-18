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
    interface_enabled = False
    
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
    else:
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
            interface_enabled = True

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
    global_vars.ui.ButtonOpenParameterRoboter.setEnabled(interface_enabled)
    global_vars.ui.ButtonDatenSenden.setEnabled(interface_enabled)
    global_vars.ui.EingabeKartonGewicht.setEnabled(interface_enabled)
    global_vars.ui.EingabeKartonhoehe.setEnabled(interface_enabled)
    global_vars.ui.EingabeStartlage.setEnabled(interface_enabled)
    global_vars.ui.checkBoxEinzelpaket.setEnabled(interface_enabled)
    global_vars.ui.checkBoxLabelInvert.setEnabled(interface_enabled)
    global_vars.ui.checkBoxKlemmung.setEnabled(interface_enabled)
    global_vars.ui.label_Gewicht.setEnabled(interface_enabled)
    global_vars.ui.label_Kartonhoehe.setEnabled(interface_enabled)
    global_vars.ui.label_Startlage.setEnabled(interface_enabled)
    global_vars.ui.label_Gewicht_kg.setEnabled(interface_enabled)
    global_vars.ui.label_Kartonhoehe_mm.setEnabled(interface_enabled)

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
    if not hasattr(global_vars, 'canvas') or not global_vars.canvas:
        logger.error("No canvas available for 3D visualization")
        return
    
    try:
        from ui_files.visualization_3d import display_pallet_3d
        
        # Get the text (name) of the selected item
        file_name = item.text()
        
        logger.info(f"Displaying 3D view of {file_name}")
        
        # Call display_pallet_3d to render the selected palette
        display_pallet_3d(global_vars.canvas, file_name)
    except Exception as e:
        logger.error(f"Failed to display file: {e}") 

def load_selected_file():
    """Load the currently selected file from the robFilesListWidget.
    
    This function gets the currently selected item from the robFilesListWidget,
    sets its text as the value of the EingabePallettenplan input field,
    and then calls the load() function to load the palette plan.
    """
    if not global_vars.ui:
        logger.error("UI not initialized")
        return
        
    # Get the selected item from the list widget
    selected_items = global_vars.ui.robFilesListWidget.selectedItems()
    if not selected_items:
        logger.warning("No palette plan selected in the list")
        return
        
    # Get the text (name) of the selected item
    file_name = selected_items[0].text()
    logger.info(f"Loading palette plan: {file_name}")
    
    # Set the value in the input field
    global_vars.ui.EingabePallettenplan.setText(file_name)
    
    # Call the load function to load the palette plan
    load() 