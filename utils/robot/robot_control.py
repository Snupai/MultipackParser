import socket
import logging
import os
from utils.system.core import global_vars
from utils.database.database import save_to_database, find_file_in_database, list_available_files
from utils.message.status_manager import update_status_label
from PySide6.QtCore import Qt, QTimer
from PySide6.QtWidgets import QDialog, QVBoxLayout, QLabel, QListWidget, QPushButton

logger = logging.getLogger(__name__)

def is_in_remote_control() -> bool:
    """Check if the robot is in remote control mode.
    
    Returns:
        bool: True if robot is in remote control mode, False otherwise
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        # Connect to the dashboard server
        server_address = (global_vars.robot_ip, 29999)
        logger.debug('connecting to %s port %s' %(server_address))
        sock.connect(server_address)
        
        # Send polyscope command to check remote control status
        message = 'is in remote control\n'
        logger.debug('sending %s' %(message))
        sock.sendall(message.encode('utf-8'))
        
        # Get response
        data = sock.recv(4096).decode('utf-8')
        logger.debug('received %s' %(data))
        
        # Response will be "true" if in remote control
        return data.strip().lower() == "true"
        
    except Exception as e:
        logger.error(f"Error checking remote control status: {e}")
        return False
    finally:
        logger.debug('closing socket')
        sock.close()

def send_remote_control_command() -> None:
    """Send the selected remote control command to the robot.
    First checks if robot is in remote control mode.
    """
    if not is_in_remote_control():
        logger.error("Cannot send command - robot not in remote control mode")
        update_status_label("Robot not in remote control mode", "red", True)
        return
        
    command = global_vars.ui.comboBoxCommandRemoteControl.currentText()
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        # Connect to the dashboard server
        server_address = (global_vars.robot_ip, 29999)
        logger.debug('connecting to %s port %s' %(server_address))
        sock.connect(server_address)
        
        # Send command
        message = command + '\n'
        logger.debug('sending %s' %(message))
        sock.sendall(message.encode('utf-8'))
        
        # Get response
        data = sock.recv(4096).decode('utf-8')
        logger.debug('received %s' %(data))
        
        if "Successfully" in data:
            update_status_label("Command sent successfully", "green", True)
        else:
            update_status_label("Error sending command", "red", True)
            
    except Exception as e:
        logger.error(f"Error sending remote control command: {e}")
        update_status_label("Error sending command", "red", True)
    finally:
        logger.debug('closing socket')
        sock.close()

def send_cmd_play() -> None:
    """Send a command to the robot to start.
    """
    if not is_in_remote_control():
        logger.error("Cannot send command - robot not in remote control mode")
        update_status_label("Robot not in remote control mode", "red", True)
        return
        
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
    if not is_in_remote_control():
        logger.error("Cannot send command - robot not in remote control mode")
        update_status_label("Robot not in remote control mode", "red", True)
        return
        
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
    if not is_in_remote_control():
        logger.error("Cannot send command - robot not in remote control mode")
        update_status_label("Robot not in remote control mode", "red", True)
        return
        
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

def Check_Einzelpaket_längs_greifen(package_length: int) -> None:
    """Automatically check if package should be gripped lengthwise based on package length.
    """
    if global_vars.ui and global_vars.ui.checkBoxEinzelpaket:
        if package_length >= 265:
            global_vars.ui.checkBoxEinzelpaket.setChecked(True)
        else:
            global_vars.ui.checkBoxEinzelpaket.setChecked(False)

def load() -> None:
    """Load the pallet plan from file.
    """
    if not global_vars.ui or not hasattr(global_vars.ui, 'EingabePallettenplan'):
        logger.error("UI not initialized")
        return
    interface_enabled = False
    
    from utils.message.status_manager import update_status_label
    import utils.server.UR_Common_functions as UR

    # Store the text first, then manually clear focus to avoid keyboard issues
    Artikelnummer = global_vars.ui.EingabePallettenplan.text().strip()
    global_vars.ui.EingabePallettenplan.clearFocus()
    
    # Validate input
    if not Artikelnummer:
        logger.warning("No palette plan number entered")
        update_status_label("Bitte Palettenplan eingeben", "red", True)
        return
        
    # Check if the input matches the expected format (numbers, hyphens, and underscores only)
    if not all(c.isdigit() or c in '-_' for c in Artikelnummer):
        logger.warning(f"Invalid palette plan number format: {Artikelnummer}")
        update_status_label("Ungültiges Format", "red", True)
        return
    
    # Check if the input exactly matches a valid palette plan
    available_files = list_available_files()
    if not available_files:
        logger.error("No palette plans found in database")
        update_status_label("Keine Palettenpläne gefunden", "red", True)
        return
        
    valid_plans = [file['file_name'].replace('.rob', '') for file in available_files]
    if Artikelnummer not in valid_plans:
        logger.warning(f"Palette plan {Artikelnummer} not found in available plans")
        update_status_label("Kein Plan gefunden", "red", True)
        return
    
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
                message_strings = ["Kein Pallettenplan geladen", "Kein Plan gefunden", "Bitte Palettenplan eingeben"]
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
            
            # Box height is already loaded from database by UR_ReadDataFromUsbStick()
            box_height = global_vars.g_PaketDim[2]
            logger.debug(f"Using box height: {box_height}")
            
            # Try to load saved weight from database first
            from utils.database.database import get_box_weight
            saved_weight = get_box_weight(global_vars.FILENAME)
            
            if saved_weight is not None:
                # Use saved weight from database
                Gewicht = saved_weight
                logger.debug(f"Loaded saved weight from database: {Gewicht}")
            else:
                # Calculate default weight based on volume
                Volumen = (global_vars.g_PaketDim[0] * global_vars.g_PaketDim[1] * box_height) / 1E+9 # in m³
                logger.debug(f"{Volumen=}")
                Dichte = 1000 # Dichte von Wasser in kg/m³
                logger.debug(f"{Dichte=}")
                Ausnutzung = 0.4 # Empirsch ermittelter Faktor - nicht für Gasflaschen
                logger.debug(f"{Ausnutzung=}")
                Gewicht = round(Volumen * Dichte * Ausnutzung, 1) # Gewicht in kg
                logger.debug(f"Calculated default weight: {Gewicht}")
            
            global_vars.ui.EingabeKartonGewicht.setText(str(Gewicht))
            global_vars.ui.EingabeKartonhoehe.setText(str(box_height))
            
            # Load saved einzelpaket_laengs setting, or use auto-check if not saved
            from utils.database.database import get_einzelpaket_laengs
            saved_einzelpaket = get_einzelpaket_laengs(global_vars.FILENAME)
            
            if saved_einzelpaket is not None:
                # Use saved setting
                global_vars.ui.checkBoxEinzelpaket.setChecked(saved_einzelpaket)
                logger.debug(f"Loaded saved einzelpaket_laengs: {saved_einzelpaket}")
            else:
                # No saved setting, use auto-check based on package length
                Check_Einzelpaket_längs_greifen(global_vars.g_PaketDim[0])
    global_vars.ui.ButtonOpenParameterRoboter.setEnabled(interface_enabled)
    global_vars.ui.ButtonDatenSenden.setEnabled(interface_enabled)
    global_vars.ui.EingabeKartonGewicht.setEnabled(interface_enabled)
    global_vars.ui.EingabeKartonhoehe.setEnabled(interface_enabled)
    global_vars.ui.EingabeStartlage.setEnabled(interface_enabled)
    global_vars.ui.checkBoxEinzelpaket.setEnabled(interface_enabled)
    global_vars.ui.checkBoxLabelInvert.setEnabled(interface_enabled)
    #global_vars.ui.checkBoxKlemmung.setEnabled(interface_enabled)
    global_vars.ui.label_Gewicht.setEnabled(interface_enabled)
    global_vars.ui.label_Kartonhoehe.setEnabled(interface_enabled)
    global_vars.ui.label_Startlage.setEnabled(interface_enabled)
    global_vars.ui.label_Gewicht_kg.setEnabled(interface_enabled)
    global_vars.ui.label_Kartonhoehe_mm.setEnabled(interface_enabled)

def _schedule_db_update_popup(new_files) -> None:
    """Accumulate updated filenames and show up to 3 popups after debounce.

    Groups rapid consecutive updates: a single popup sequence appears after
    a short quiet period, listing all unique filenames (split across max 3 popups).
    """
    if not (getattr(global_vars, 'main_window', None) and global_vars.main_window.isVisible()):
        return

    # Initialize accumulator
    if not hasattr(global_vars, 'db_update_accumulator') or global_vars.db_update_accumulator is None:
        global_vars.db_update_accumulator = set()

    # Add new files
    for f in new_files:
        global_vars.db_update_accumulator.add(f)

    # Create or restart debounce timer
    def show_popups():
        try:
            files = sorted(global_vars.db_update_accumulator) if getattr(global_vars, 'db_update_accumulator', None) else []
            global_vars.db_update_accumulator = set()  # reset
            if not files:
                return

            # Single scrollable dialog with all filenames
            dialog = QDialog(global_vars.main_window)
            dialog.setWindowTitle("Datenbank aktualisiert")
            layout = QVBoxLayout(dialog)

            title = QLabel("<b>Folgende Dateien wurden aktualisiert/neu hinzugefügt:</b>")
            title.setTextFormat(Qt.TextFormat.RichText)
            layout.addWidget(title)

            list_widget = QListWidget(dialog)
            # Populate list
            for fn in files:
                list_widget.addItem(fn)
            # Make sure it's scrollable by constraining size
            list_widget.setMinimumSize(500, 300)
            layout.addWidget(list_widget)

            ok_button = QPushButton("OK", dialog)
            ok_button.clicked.connect(dialog.accept)
            layout.addWidget(ok_button)

            dialog.setMinimumSize(520, 380)
            dialog.exec()
        finally:
            # Clean up timer reference
            if getattr(global_vars, 'db_update_timer', None):
                global_vars.db_update_timer.stop()
                global_vars.db_update_timer.deleteLater()
                global_vars.db_update_timer = None

    # Restart single-shot timer (debounce 2000 ms)
    if getattr(global_vars, 'db_update_timer', None) is None:
        global_vars.db_update_timer = QTimer(global_vars.main_window)
        global_vars.db_update_timer.setSingleShot(True)
        global_vars.db_update_timer.timeout.connect(show_popups)
    else:
        global_vars.db_update_timer.stop()
    global_vars.db_update_timer.start(2000)


def update_database_from_usb() -> None:
    """Update the database with any new or modified palette plans from the USB stick.

    Provides UI feedback when the main window is available:
    - Shows a blinking orange status while updating
    - Sets a wait cursor during the operation
    - Shows a green status on completion or red on errors
    """
    # Determine if we can give UI feedback (only when main window exists and is visible)
    ui_ready = bool(getattr(global_vars, 'main_window', None)) and global_vars.main_window.isVisible()

    try:
        if not os.path.exists(global_vars.PATH_USB_STICK):
            logger.error(f"USB stick path {global_vars.PATH_USB_STICK} does not exist")
            # Do not show status messages; silently return if no UI
            return

        # Start UI feedback
        if ui_ready:
            try:
                global_vars.main_window.setCursor(Qt.CursorShape.WaitCursor)
            except Exception:
                pass

        # Get all .rob files
        rob_files = [f for f in os.listdir(global_vars.PATH_USB_STICK) if f.endswith(".rob")]
        logger.info(f"Found {len(rob_files)} .rob files to process")
        updated_files = []

        # Session cache of failed files to avoid retry loops
        if not hasattr(global_vars, 'failed_rob_files') or global_vars.failed_rob_files is None:
            global_vars.failed_rob_files = set()

        for file in rob_files:
            # Skip files previously detected as broken in this session
            if file in getattr(global_vars, 'failed_rob_files', set()):
                logger.debug(f"Skipping previously failed file: {file}")
                continue
            file_path = os.path.join(global_vars.PATH_USB_STICK, file)
            file_timestamp = os.path.getmtime(file_path)

            # Check if file exists in database and compare timestamps
            db_file = find_file_in_database(file)
            should_update = True

            if db_file:
                db_timestamp = db_file.get('timestamp', 0)
                if file_timestamp <= db_timestamp:
                    logger.debug(f"File {file} is up to date in database")
                    should_update = False

            # Update if file is new or modified
            if should_update:
                logger.info(f"Processing file: {file}")
                try:
                    db_manager = getattr(global_vars, 'db_manager', None)
                    saved = save_to_database(file, db_manager=db_manager)
                    if saved:
                        updated_files.append(file)
                    else:
                        # Mark as failed to avoid repeated attempts within this session
                        getattr(global_vars, 'failed_rob_files', set()).add(file)
                        logger.warning(f"File '{file}' not saved (parse/validation failed). Will be skipped for this session.")
                except Exception as e:
                    logger.error(f"Error processing {file}: {e}")
                    # Mark as failed to avoid repeated attempts within this session
                    getattr(global_vars, 'failed_rob_files', set()).add(file)

        # Update UI 3D list if needed
        if hasattr(global_vars, 'ui') and global_vars.ui:
            from ui_files.visualization_3d import load_rob_files
            load_rob_files()

        # After processing, batch filenames into the grouped popup flow (debounced)
        if ui_ready and updated_files:
            _schedule_db_update_popup(updated_files)

    except Exception as e:
        logger.error(f"Unexpected error while updating database: {e}")
    finally:
        if ui_ready:
            try:
                global_vars.main_window.setCursor(Qt.CursorShape.ArrowCursor)
            except Exception:
                pass

def load_wordlist() -> list:
    """Load the wordlist from the USB stick and update the database.

    Returns:
        list: A list of wordlist items.
    """
    wordlist = []
    count = 0
    
    # First update the database with any new or modified files
    update_database_from_usb()
    
    # Then load the wordlist
    for file in os.listdir(global_vars.PATH_USB_STICK):
        if file.endswith(".rob"):
            wordlist.append(file[:-4])
            count = count + 1
            
    # Sort the wordlist alphabetically
    wordlist.sort()
            
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