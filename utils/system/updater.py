import logging
import os
import shutil
import subprocess
import sys
from PySide6.QtWidgets import QMessageBox

from utils.system.core import global_vars
from utils.system.core.app_control import exit_app

logger = logging.getLogger(__name__)

def check_for_updates():
    """Check for updates.
    """    
    
    # TODO: Add visual feedback to the user so that they know that the application is checking for updates
    # disable the main window for interaction
    global_vars.main_window.setDisabled(True)
    msg_box = QMessageBox()
    msg_box.setWindowTitle("Update suchen")
    msg_box.setText("Es wird nach einem Update gesucht.<br>Bitte warten Sie, während der Update-Prozess ausgeführt wird.")
    msg_box.setIcon(QMessageBox.Icon.Information)
    msg_box.show()
    
    search_paths = ["/media", "/mnt"]
    update_file_name = "MultipackParser"
    found_update_file = None

    # Traverse /media and /mnt to find the update file
    for base_path in search_paths:
        for root, dirs, files in os.walk(base_path):
            if update_file_name in files:
                found_update_file = os.path.join(root, update_file_name)
                break
        if found_update_file:
            break

    if not found_update_file:
        logger.info("No update file found.")
        # enable the main window for interaction
        global_vars.main_window.setDisabled(False)
        # show a message box to the user that no update file was found
        msg_box.setWindowTitle("Keine Updates gefunden")
        msg_box.setText("Es wurden keine Updates gefunden. Die aktuelle Version ist die neueste Version.")
        msg_box.exec()
        return

    logger.info(f"Update file found: {found_update_file}")
    # show a message box to the user that an update was found
    msg_box.setWindowTitle("Update gefunden")
    msg_box.setText("Es wurde ein Update gefunden.<br>Bitte warten Sie, während der Update-Prozess ausgeführt wird.")

    # copy the new binary to cwd/update/MultipackParser and make it executable
    os.makedirs("update", exist_ok=True)
    shutil.copy(found_update_file, "update/MultipackParser")
    os.chmod("update/MultipackParser", 0o755)
    found_update_file = f"{os.getcwd()}/update/MultipackParser"
    logger.debug("New binary copied to update/MultipackParser")
    logger.debug("New binary is executable")
    logger.debug("Checking for new Version")
    
    # run the new binary with --version to check for version string and compare it to the global_arg.VERSION
    new_version = subprocess.check_output([f"{found_update_file}", "--version"]).decode().strip()
    # the version string is in the format "Multipack Parser Application Version: 1.5.3-beta"
    new_version = new_version.split(" ")[-1]
    logger.debug(f"New version: {new_version}")
    
    current_version = global_vars.VERSION.split("-")
    current_version_tag = current_version[1] if len(current_version) > 1 else None
    current_version = current_version[0].split(".")
    
    new_version = new_version.split("-")
    new_version_tag = new_version[1] if len(new_version) > 1 else None
    new_version = new_version[0].split(".")
    
    # Convert str[] to int[]
    current_version = [int(x) for x in current_version]
    new_version = [int(x) for x in new_version]

    # Compare versions
    if new_version[0] <= current_version[0] and new_version[1] <= current_version[1] and new_version[2] <= current_version[2]:
        logger.info("No new version found.")
        # change the text of the message box to "Die aktuelle Version ist die neuere Version."
        msg_box.setText("Die aktuelle Version ist die neuere Version.")
        return
    
    logger.info(f"New version found: {new_version}")
    # change the text of the message box to "Update gefunden. Version {new_version} ist verfügbar.<br>Bitte warten Sie, während der Update-Prozess ausgeführt wird."
    msg_box.setText(f"Update gefunden. Version {new_version} wird installiert.<br>Bitte warten Sie, während der Update-Prozess ausgeführt wird.")
    # Spawn an updater process
    current_binary = sys.argv[0]  # Path to the running binary
    updater_script = f"""#!/bin/bash
# Wait for the parent process to terminate
sleep 5

# Replace the current binary with the update file
cp "{found_update_file}" "{current_binary}"
chmod +x "{current_binary}"  # Ensure the binary is executable

rm -rf "{os.getcwd()}/update"

# Optionally reboot the system immediately
reboot
"""

    # Write the updater script to a temporary file
    updater_path = f"{os.getcwd()}/update/updater_script.sh"
    with open(updater_path, "w") as f:
        f.write(updater_script)

    # Make the script executable
    os.chmod(updater_path, 0o755)  # Ensure the script is executable

    # Spawn the updater process
    subprocess.Popen(  # Spawn the updater process
        ["/bin/bash", updater_path],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        close_fds=True,
        start_new_session=True,  # Detach the process
    )

    logger.info("Updater process spawned. Exiting current application.")  # Log the process spawn
    exit_app() 